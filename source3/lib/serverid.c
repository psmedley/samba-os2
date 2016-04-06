/*
   Unix SMB/CIFS implementation.
   Implementation of a reliable server_exists()
   Copyright (C) Volker Lendecke 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "serverid.h"
#include "util_tdb.h"
#include "dbwrap/dbwrap.h"
#include "dbwrap/dbwrap_open.h"
#include "lib/tdb_wrap/tdb_wrap.h"
#include "lib/param/param.h"
#include "ctdbd_conn.h"
#include "messages.h"
#include "lib/messages_dgm.h"

struct serverid_key {
	pid_t pid;
	uint32_t task_id;
	uint32_t vnn;
};

struct serverid_data {
	uint64_t unique_id;
	uint32_t msg_flags;
};

static struct db_context *serverid_db(void)
{
	static struct db_context *db;
	char *db_path;

	if (db != NULL) {
		return db;
	}

	db_path = lock_path("serverid.tdb");
	if (db_path == NULL) {
		return NULL;
	}

	db = db_open(NULL, db_path, 0,
		     TDB_DEFAULT|TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
		     O_RDWR|O_CREAT, 0644, DBWRAP_LOCK_ORDER_2,
		     DBWRAP_FLAG_NONE);
	TALLOC_FREE(db_path);
	return db;
}

bool serverid_parent_init(TALLOC_CTX *mem_ctx)
{
	struct db_context *db;

	db = serverid_db();
	if (db == NULL) {
		DEBUG(1, ("could not open serverid.tdb: %s\n",
			  strerror(errno)));
		return false;
	}

	return true;
}

static void serverid_fill_key(const struct server_id *id,
			      struct serverid_key *key)
{
	ZERO_STRUCTP(key);
	key->pid = id->pid;
	key->task_id = id->task_id;
	key->vnn = id->vnn;
}

bool serverid_register(const struct server_id id, uint32_t msg_flags)
{
	struct db_context *db;
	struct serverid_key key;
	struct serverid_data data;
	struct db_record *rec;
	TDB_DATA tdbkey, tdbdata;
	NTSTATUS status;
	bool ret = false;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}

	serverid_fill_key(&id, &key);
	tdbkey = make_tdb_data((uint8_t *)&key, sizeof(key));

	rec = dbwrap_fetch_locked(db, talloc_tos(), tdbkey);
	if (rec == NULL) {
		DEBUG(1, ("Could not fetch_lock serverid.tdb record\n"));
		return false;
	}

	ZERO_STRUCT(data);
	data.unique_id = id.unique_id;
	data.msg_flags = msg_flags;

	tdbdata = make_tdb_data((uint8_t *)&data, sizeof(data));
	status = dbwrap_record_store(rec, tdbdata, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Storing serverid.tdb record failed: %s\n",
			  nt_errstr(status)));
		goto done;
	}

	if (lp_clustering()) {
		register_with_ctdbd(messaging_ctdbd_connection(), id.unique_id,
				    NULL, NULL);
	}

	ret = true;
done:
	TALLOC_FREE(rec);
	return ret;
}

bool serverid_deregister(struct server_id id)
{
	struct db_context *db;
	struct serverid_key key;
	struct db_record *rec;
	TDB_DATA tdbkey;
	NTSTATUS status;
	bool ret = false;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}

	serverid_fill_key(&id, &key);
	tdbkey = make_tdb_data((uint8_t *)&key, sizeof(key));

	rec = dbwrap_fetch_locked(db, talloc_tos(), tdbkey);
	if (rec == NULL) {
		DEBUG(1, ("Could not fetch_lock serverid.tdb record\n"));
		return false;
	}

	status = dbwrap_record_delete(rec);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Deleting serverid.tdb record failed: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	ret = true;
done:
	TALLOC_FREE(rec);
	return ret;
}

static bool serverid_exists_local(const struct server_id *id)
{
	bool exists = process_exists_by_pid(id->pid);
	uint64_t unique;
	int ret;

	if (!exists) {
		return false;
	}

	if (id->unique_id == SERVERID_UNIQUE_ID_NOT_TO_VERIFY) {
		return true;
	}

	ret = messaging_dgm_get_unique(id->pid, &unique);
	if (ret != 0) {
		return false;
	}

	return (unique == id->unique_id);
}

bool serverid_exists(const struct server_id *id)
{
	if (procid_is_local(id)) {
		return serverid_exists_local(id);
	}

	if (lp_clustering()) {
		return ctdbd_process_exists(messaging_ctdbd_connection(),
					    id->vnn, id->pid);
	}

	return false;
}

static bool serverid_rec_parse(const struct db_record *rec,
			       struct server_id *id, uint32_t *msg_flags)
{
	struct serverid_key key;
	struct serverid_data data;
	TDB_DATA tdbkey;
	TDB_DATA tdbdata;

	tdbkey = dbwrap_record_get_key(rec);
	tdbdata = dbwrap_record_get_value(rec);

	if (tdbkey.dsize != sizeof(key)) {
		DEBUG(1, ("Found invalid key length %d in serverid.tdb\n",
			  (int)tdbkey.dsize));
		return false;
	}
	if (tdbdata.dsize != sizeof(data)) {
		DEBUG(1, ("Found invalid value length %d in serverid.tdb\n",
			  (int)tdbdata.dsize));
		return false;
	}

	memcpy(&key, tdbkey.dptr, sizeof(key));
	memcpy(&data, tdbdata.dptr, sizeof(data));

	id->pid = key.pid;
	id->task_id = key.task_id;
	id->vnn = key.vnn;
	id->unique_id = data.unique_id;
	*msg_flags = data.msg_flags;
	return true;
}

struct serverid_traverse_read_state {
	int (*fn)(const struct server_id *id, uint32_t msg_flags,
		  void *private_data);
	void *private_data;
};

static int serverid_traverse_read_fn(struct db_record *rec, void *private_data)
{
	struct serverid_traverse_read_state *state =
		(struct serverid_traverse_read_state *)private_data;
	struct server_id id;
	uint32_t msg_flags;

	if (!serverid_rec_parse(rec, &id, &msg_flags)) {
		return 0;
	}
	return state->fn(&id, msg_flags,state->private_data);
}

bool serverid_traverse_read(int (*fn)(const struct server_id *id,
				      uint32_t msg_flags, void *private_data),
			    void *private_data)
{
	struct db_context *db;
	struct serverid_traverse_read_state state;
	NTSTATUS status;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}
	state.fn = fn;
	state.private_data = private_data;

	status = dbwrap_traverse_read(db, serverid_traverse_read_fn, &state,
				      NULL);
	return NT_STATUS_IS_OK(status);
}

struct serverid_traverse_state {
	int (*fn)(struct db_record *rec, const struct server_id *id,
		  uint32_t msg_flags, void *private_data);
	void *private_data;
};

static int serverid_traverse_fn(struct db_record *rec, void *private_data)
{
	struct serverid_traverse_state *state =
		(struct serverid_traverse_state *)private_data;
	struct server_id id;
	uint32_t msg_flags;

	if (!serverid_rec_parse(rec, &id, &msg_flags)) {
		return 0;
	}
	return state->fn(rec, &id, msg_flags, state->private_data);
}

bool serverid_traverse(int (*fn)(struct db_record *rec,
				 const struct server_id *id,
				 uint32_t msg_flags, void *private_data),
			    void *private_data)
{
	struct db_context *db;
	struct serverid_traverse_state state;
	NTSTATUS status;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}
	state.fn = fn;
	state.private_data = private_data;

	status = dbwrap_traverse(db, serverid_traverse_fn, &state, NULL);
	return NT_STATUS_IS_OK(status);
}
