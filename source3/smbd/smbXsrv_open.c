/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2012
   Copyright (C) Michael Adam 2012

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

#include "smbXsrv_open.h"
#include "includes.h"
#include "system/filesys.h"
#include "lib/util/server_id.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "dbwrap/dbwrap.h"
#include "dbwrap/dbwrap_rbt.h"
#include "dbwrap/dbwrap_open.h"
#include "../libcli/security/security.h"
#include "messages.h"
#include "lib/util/util_tdb.h"
#include "librpc/gen_ndr/ndr_smbXsrv.h"
#include "serverid.h"
#include "source3/include/util_tdb.h"
#include "lib/util/idtree_random.h"
#include "lib/util/time_basic.h"

struct smbXsrv_open_table {
	struct {
		struct idr_context *idr;
		struct db_context *replay_cache_db_ctx;
		uint32_t lowest_id;
		uint32_t highest_id;
		uint32_t max_opens;
		uint32_t num_opens;
	} local;
	struct {
		struct db_context *db_ctx;
	} global;
};

static struct db_context *smbXsrv_open_global_db_ctx = NULL;

NTSTATUS smbXsrv_open_global_init(void)
{
	char *global_path = NULL;
	struct db_context *db_ctx = NULL;

	if (smbXsrv_open_global_db_ctx != NULL) {
		return NT_STATUS_OK;
	}

	global_path = lock_path(talloc_tos(), "smbXsrv_open_global.tdb");
	if (global_path == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	db_ctx = db_open(NULL, global_path,
			 SMBD_VOLATILE_TDB_HASH_SIZE,
			 SMBD_VOLATILE_TDB_FLAGS,
			 O_RDWR | O_CREAT, 0600,
			 DBWRAP_LOCK_ORDER_1,
			 DBWRAP_FLAG_NONE);
	TALLOC_FREE(global_path);
	if (db_ctx == NULL) {
		NTSTATUS status;

		status = map_nt_error_from_unix_common(errno);

		return status;
	}

	smbXsrv_open_global_db_ctx = db_ctx;

	return NT_STATUS_OK;
}

/*
 * NOTE:
 * We need to store the keys in big endian so that dbwrap_rbt's memcmp
 * has the same result as integer comparison between the uint32_t
 * values.
 *
 * TODO: implement string based key
 */

struct smbXsrv_open_global_key_buf { uint8_t buf[sizeof(uint32_t)]; };

static TDB_DATA smbXsrv_open_global_id_to_key(
	uint32_t id, struct smbXsrv_open_global_key_buf *key_buf)
{
	RSIVAL(key_buf->buf, 0, id);

	return (TDB_DATA) {
		.dptr = key_buf->buf,
		.dsize = sizeof(key_buf->buf),
	};
}

static NTSTATUS smbXsrv_open_table_init(struct smbXsrv_connection *conn,
					uint32_t lowest_id,
					uint32_t highest_id,
					uint32_t max_opens)
{
	struct smbXsrv_client *client = conn->client;
	struct smbXsrv_open_table *table;
	NTSTATUS status;
	uint64_t max_range;

	if (lowest_id > highest_id) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	max_range = highest_id;
	max_range -= lowest_id;
	max_range += 1;

	if (max_opens > max_range) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	table = talloc_zero(client, struct smbXsrv_open_table);
	if (table == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	table->local.idr = idr_init(table);
	if (table->local.idr == NULL) {
		TALLOC_FREE(table);
		return NT_STATUS_NO_MEMORY;
	}
	table->local.replay_cache_db_ctx = db_open_rbt(table);
	if (table->local.replay_cache_db_ctx == NULL) {
		TALLOC_FREE(table);
		return NT_STATUS_NO_MEMORY;
	}
	table->local.lowest_id = lowest_id;
	table->local.highest_id = highest_id;
	table->local.max_opens = max_opens;

	status = smbXsrv_open_global_init();
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(table);
		return status;
	}

	table->global.db_ctx = smbXsrv_open_global_db_ctx;

	client->open_table = table;
	return NT_STATUS_OK;
}

static NTSTATUS smbXsrv_open_local_lookup(struct smbXsrv_open_table *table,
					  uint32_t open_local_id,
					  uint32_t open_global_id,
					  NTTIME now,
					  struct smbXsrv_open **_open)
{
	struct smbXsrv_open *op = NULL;

	*_open = NULL;

	if (open_local_id == 0) {
		return NT_STATUS_FILE_CLOSED;
	}

	if (table == NULL) {
		/* this might happen before the end of negprot */
		return NT_STATUS_FILE_CLOSED;
	}

	if (table->local.idr == NULL) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	op = idr_find(table->local.idr, open_local_id);
	if (op == NULL) {
		return NT_STATUS_FILE_CLOSED;
	}

	if (open_global_id == 0) {
		/* make the global check a no-op for SMB1 */
		open_global_id = op->global->open_global_id;
	}

	if (op->global->open_global_id != open_global_id) {
		return NT_STATUS_FILE_CLOSED;
	}

	if (now != 0) {
		op->idle_time = now;
	}

	*_open = op;
	return NT_STATUS_OK;
}

static NTSTATUS smbXsrv_open_global_parse_record(
	TALLOC_CTX *mem_ctx,
	TDB_DATA key,
	TDB_DATA val,
	struct smbXsrv_open_global0 **global)
{
	DATA_BLOB blob = data_blob_const(val.dptr, val.dsize);
	struct smbXsrv_open_globalB global_blob;
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	TALLOC_CTX *frame = talloc_stackframe();

	ndr_err = ndr_pull_struct_blob(&blob, frame, &global_blob,
			(ndr_pull_flags_fn_t)ndr_pull_smbXsrv_open_globalB);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("Invalid record in smbXsrv_open_global.tdb:"
			 "key '%s' ndr_pull_struct_blob - %s\n",
			 tdb_data_dbg(key),
			 ndr_errstr(ndr_err)));
		status = ndr_map_error2ntstatus(ndr_err);
		goto done;
	}

	DBG_DEBUG("\n");
	if (CHECK_DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(smbXsrv_open_globalB, &global_blob);
	}

	if (global_blob.version != SMBXSRV_VERSION_0) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		DEBUG(1,("Invalid record in smbXsrv_open_global.tdb:"
			 "key '%s' unsupported version - %d - %s\n",
			 tdb_data_dbg(key),
			 (int)global_blob.version,
			 nt_errstr(status)));
		goto done;
	}

	if (global_blob.info.info0 == NULL) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		DEBUG(1,("Invalid record in smbXsrv_open_global.tdb:"
			 "key '%s' info0 NULL pointer - %s\n",
			 tdb_data_dbg(key),
			 nt_errstr(status)));
		goto done;
	}

	*global = talloc_move(mem_ctx, &global_blob.info.info0);
	status = NT_STATUS_OK;
done:
	talloc_free(frame);
	return status;
}

static NTSTATUS smbXsrv_open_global_verify_record(
	TDB_DATA key,
	TDB_DATA val,
	TALLOC_CTX *mem_ctx,
	struct smbXsrv_open_global0 **_global0)
{
	struct smbXsrv_open_global0 *global0 = NULL;
	struct server_id_buf buf;
	NTSTATUS status;

	if (val.dsize == 0) {
		return NT_STATUS_NOT_FOUND;
	}

	status = smbXsrv_open_global_parse_record(mem_ctx, key, val, &global0);
	if (!NT_STATUS_IS_OK(status)) {
		DBG_WARNING("smbXsrv_open_global_parse_record for %s failed: "
			    "%s\n",
			    tdb_data_dbg(key),
			    nt_errstr(status));
		return status;
	}
	*_global0 = global0;

	if (server_id_is_disconnected(&global0->server_id)) {
		return NT_STATUS_OK;
	}
	if (serverid_exists(&global0->server_id)) {
		return NT_STATUS_OK;
	}

	DBG_WARNING("smbd %s did not clean up record %s\n",
		    server_id_str_buf(global0->server_id, &buf),
		    tdb_data_dbg(key));

	return NT_STATUS_FATAL_APP_EXIT;
}

static NTSTATUS smbXsrv_open_global_store(
	struct db_record *rec,
	TDB_DATA key,
	TDB_DATA oldval,
	struct smbXsrv_open_global0 *global)
{
	struct smbXsrv_open_globalB global_blob;
	DATA_BLOB blob = data_blob_null;
	TDB_DATA val = { .dptr = NULL, };
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	/*
	 * TODO: if we use other versions than '0'
	 * we would add glue code here, that would be able to
	 * store the information in the old format.
	 */

	global_blob = (struct smbXsrv_open_globalB) {
		.version = smbXsrv_version_global_current(),
	};

	if (oldval.dsize >= 8) {
		global_blob.seqnum = IVAL(oldval.dptr, 4);
	}
	global_blob.seqnum += 1;
	global_blob.info.info0 = global;

	ndr_err = ndr_push_struct_blob(&blob, talloc_tos(), &global_blob,
			(ndr_push_flags_fn_t)ndr_push_smbXsrv_open_globalB);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DBG_WARNING("key '%s' ndr_push - %s\n",
			    tdb_data_dbg(key),
			    ndr_map_error2string(ndr_err));
		return ndr_map_error2ntstatus(ndr_err);
	}

	val = make_tdb_data(blob.data, blob.length);
	status = dbwrap_record_store(rec, val, TDB_REPLACE);
	TALLOC_FREE(blob.data);
	if (!NT_STATUS_IS_OK(status)) {
		DBG_WARNING("key '%s' store - %s\n",
			    tdb_data_dbg(key),
			    nt_errstr(status));
		return status;
	}

	if (CHECK_DEBUGLVL(10)) {
		DBG_DEBUG("key '%s' stored\n", tdb_data_dbg(key));
		NDR_PRINT_DEBUG(smbXsrv_open_globalB, &global_blob);
	}

	return NT_STATUS_OK;
}

struct smbXsrv_open_global_allocate_state {
	uint32_t id;
	struct smbXsrv_open_global0 *global;
	NTSTATUS status;
};

static void smbXsrv_open_global_allocate_fn(
	struct db_record *rec, TDB_DATA oldval, void *private_data)
{
	struct smbXsrv_open_global_allocate_state *state = private_data;
	struct smbXsrv_open_global0 *global = state->global;
	struct smbXsrv_open_global0 *tmp_global0 = NULL;
	TDB_DATA key = dbwrap_record_get_key(rec);

	state->status = smbXsrv_open_global_verify_record(
		key, oldval, talloc_tos(), &tmp_global0);

	if (NT_STATUS_IS_OK(state->status)) {
		/*
		 * Found an existing record
		 */
		TALLOC_FREE(tmp_global0);
		state->status = NT_STATUS_RETRY;
		return;
	}

	if (NT_STATUS_EQUAL(state->status, NT_STATUS_NOT_FOUND)) {
		/*
		 * Found an empty slot
		 */
		global->open_global_id = state->id;
		global->open_persistent_id = state->id;

		state->status = smbXsrv_open_global_store(
			rec, key, (TDB_DATA) { .dsize = 0, }, state->global);
		if (!NT_STATUS_IS_OK(state->status)) {
			DBG_WARNING("smbXsrv_open_global_store() for "
				    "id %"PRIu32" failed: %s\n",
				    state->id,
				    nt_errstr(state->status));
		}
		return;
	}

	if (NT_STATUS_EQUAL(state->status, NT_STATUS_FATAL_APP_EXIT)) {
		NTSTATUS status;

		TALLOC_FREE(tmp_global0);

		/*
		 * smbd crashed
		 */
		status = dbwrap_record_delete(rec);
		if (!NT_STATUS_IS_OK(status)) {
			DBG_WARNING("dbwrap_record_delete() failed "
				    "for record %"PRIu32": %s\n",
				    state->id,
				    nt_errstr(status));
			state->status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			return;
		}
		return;
	}
}

static NTSTATUS smbXsrv_open_global_allocate(
	struct db_context *db, struct smbXsrv_open_global0 *global)
{
	struct smbXsrv_open_global_allocate_state state = {
		.global = global,
	};
	uint32_t i;
	uint32_t last_free = 0;
	const uint32_t min_tries = 3;

	/*
	 * Here we just randomly try the whole 32-bit space
	 *
	 * We use just 32-bit, because we want to reuse the
	 * ID for SRVSVC.
	 */
	for (i = 0; i < UINT32_MAX; i++) {
		struct smbXsrv_open_global_key_buf key_buf;
		TDB_DATA key;
		NTSTATUS status;

		if (i >= min_tries && last_free != 0) {
			state.id = last_free;
		} else {
			generate_nonce_buffer(
				(uint8_t *)&state.id, sizeof(state.id));
			state.id = MAX(state.id, 1);
			state.id = MIN(state.id, UINT32_MAX-1);
		}

		key = smbXsrv_open_global_id_to_key(state.id, &key_buf);

		status = dbwrap_do_locked(
			db, key, smbXsrv_open_global_allocate_fn, &state);

		if (!NT_STATUS_IS_OK(status)) {
			DBG_WARNING("dbwrap_do_locked() failed: %s\n",
				    nt_errstr(status));
			return NT_STATUS_INTERNAL_DB_ERROR;
		}

		if (NT_STATUS_IS_OK(state.status)) {
			/*
			 * Found an empty slot, done.
			 */
			DBG_DEBUG("Found slot %"PRIu32"\n", state.id);
			return NT_STATUS_OK;
		}

		if (NT_STATUS_EQUAL(state.status, NT_STATUS_FATAL_APP_EXIT)) {

			if ((i < min_tries) && (last_free == 0)) {
				/*
				 * Remember "id" as free but also try
				 * others to not recycle ids too
				 * quickly.
				 */
				last_free = state.id;
			}
			continue;
		}

		if (NT_STATUS_EQUAL(state.status, NT_STATUS_RETRY)) {
			/*
			 * Normal collision, try next
			 */
			DBG_DEBUG("Found record for id %"PRIu32"\n",
				  state.id);
			continue;
		}

		DBG_WARNING("smbXsrv_open_global_allocate_fn() failed: %s\n",
			    nt_errstr(state.status));
		return state.status;
	}

	/* should not be reached */
	return NT_STATUS_INTERNAL_ERROR;
}

static int smbXsrv_open_destructor(struct smbXsrv_open *op)
{
	NTSTATUS status;

	status = smbXsrv_open_close(op, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("smbXsrv_open_destructor: "
			  "smbXsrv_open_close() failed - %s\n",
			  nt_errstr(status)));
	}

	TALLOC_FREE(op->global);

	return 0;
}

NTSTATUS smbXsrv_open_create(struct smbXsrv_connection *conn,
			     struct auth_session_info *session_info,
			     NTTIME now,
			     struct smbXsrv_open **_open)
{
	struct smbXsrv_open_table *table = conn->client->open_table;
	struct smbXsrv_open *op = NULL;
	struct smbXsrv_open_global0 *global = NULL;
	NTSTATUS status;
	struct dom_sid *current_sid = NULL;
	struct security_token *current_token = NULL;
	int local_id;

	if (session_info == NULL) {
		return NT_STATUS_INVALID_HANDLE;
	}
	current_token = session_info->security_token;

	if (current_token == NULL) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (current_token->num_sids > PRIMARY_USER_SID_INDEX) {
		current_sid = &current_token->sids[PRIMARY_USER_SID_INDEX];
	}

	if (current_sid == NULL) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (table->local.num_opens >= table->local.max_opens) {
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}

	op = talloc_zero(table, struct smbXsrv_open);
	if (op == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	op->table = table;
	op->status = NT_STATUS_OK; /* TODO: start with INTERNAL_ERROR */
	op->idle_time = now;

	global = talloc_zero(op, struct smbXsrv_open_global0);
	if (global == NULL) {
		TALLOC_FREE(op);
		return NT_STATUS_NO_MEMORY;
	}
	op->global = global;

	/*
	 * We mark every slot as invalid using 0xFF.
	 * Valid values are masked with 0xF.
	 */
	memset(global->lock_sequence_array, 0xFF,
	       sizeof(global->lock_sequence_array));

	local_id = idr_get_new_random(
		table->local.idr,
		op,
		table->local.lowest_id,
		table->local.highest_id);
	if (local_id == -1) {
		TALLOC_FREE(op);
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}
	op->local_id = local_id;

	global->open_volatile_id = op->local_id;

	global->server_id = messaging_server_id(conn->client->msg_ctx);
	global->open_time = now;
	global->open_owner = *current_sid;
	if (conn->protocol >= PROTOCOL_SMB2_10) {
		global->client_guid = conn->smb2.client.guid;
	}

	status = smbXsrv_open_global_allocate(table->global.db_ctx,
					      global);
	if (!NT_STATUS_IS_OK(status)) {
		int ret = idr_remove(table->local.idr, local_id);
		SMB_ASSERT(ret == 0);

		DBG_WARNING("smbXsrv_open_global_allocate() failed: %s\n",
			    nt_errstr(status));
		TALLOC_FREE(op);
		return status;
	}

	table->local.num_opens += 1;
	talloc_set_destructor(op, smbXsrv_open_destructor);

	if (CHECK_DEBUGLVL(10)) {
		struct smbXsrv_openB open_blob = {
			.version = SMBXSRV_VERSION_0,
			.info.info0 = op,
		};

		DEBUG(10,("smbXsrv_open_create: global_id (0x%08x) stored\n",
			 op->global->open_global_id));
		NDR_PRINT_DEBUG(smbXsrv_openB, &open_blob);
	}

	*_open = op;
	return NT_STATUS_OK;
}

static NTSTATUS smbXsrv_open_set_replay_cache(struct smbXsrv_open *op)
{
	struct GUID *create_guid;
	struct GUID_txt_buf buf;
	char *guid_string;
	struct db_context *db = op->table->local.replay_cache_db_ctx;
	struct smbXsrv_open_replay_cache rc = {
		.idle_time = op->idle_time,
		.local_id = op->local_id,
	};
	uint8_t data[SMBXSRV_OPEN_REPLAY_CACHE_FIXED_SIZE] = { 0 };
	DATA_BLOB blob = { .data = data, .length = sizeof(data), };
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	TDB_DATA val;

	if (!(op->flags & SMBXSRV_OPEN_NEED_REPLAY_CACHE)) {
		return NT_STATUS_OK;
	}

	if (op->flags & SMBXSRV_OPEN_HAVE_REPLAY_CACHE) {
		return NT_STATUS_OK;
	}

	create_guid = &op->global->create_guid;
	guid_string = GUID_buf_string(create_guid, &buf);

	ndr_err = ndr_push_struct_into_fixed_blob(&blob, &rc,
		(ndr_push_flags_fn_t)ndr_push_smbXsrv_open_replay_cache);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		return status;
	}
	val = make_tdb_data(blob.data, blob.length);

	status = dbwrap_store_bystring(db, guid_string, val, TDB_REPLACE);

	if (NT_STATUS_IS_OK(status)) {
		op->flags |= SMBXSRV_OPEN_HAVE_REPLAY_CACHE;
		op->flags &= ~SMBXSRV_OPEN_NEED_REPLAY_CACHE;
	}

	return status;
}

NTSTATUS smbXsrv_open_purge_replay_cache(struct smbXsrv_client *client,
					 const struct GUID *create_guid)
{
	struct GUID_txt_buf buf;
	char *guid_string;
	struct db_context *db;

	if (client->open_table == NULL) {
		return NT_STATUS_OK;
	}

	db = client->open_table->local.replay_cache_db_ctx;

	guid_string = GUID_buf_string(create_guid, &buf);
	if (guid_string == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	return dbwrap_purge_bystring(db, guid_string);
}

static NTSTATUS smbXsrv_open_clear_replay_cache(struct smbXsrv_open *op)
{
	struct GUID *create_guid;
	struct GUID_txt_buf buf;
	char *guid_string;
	struct db_context *db;
	NTSTATUS status;

	if (op->table == NULL) {
		return NT_STATUS_OK;
	}

	db = op->table->local.replay_cache_db_ctx;

	if (!(op->flags & SMBXSRV_OPEN_HAVE_REPLAY_CACHE)) {
		return NT_STATUS_OK;
	}

	create_guid = &op->global->create_guid;
	if (GUID_all_zero(create_guid)) {
		return NT_STATUS_OK;
	}

	guid_string = GUID_buf_string(create_guid, &buf);
	if (guid_string == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dbwrap_purge_bystring(db, guid_string);

	if (NT_STATUS_IS_OK(status)) {
		op->flags &= ~SMBXSRV_OPEN_HAVE_REPLAY_CACHE;
	}

	return status;
}

struct smbXsrv_open_update_state {
	struct smbXsrv_open_global0 *global;
	NTSTATUS status;
};

static void smbXsrv_open_update_fn(
	struct db_record *rec, TDB_DATA oldval, void *private_data)
{
	struct smbXsrv_open_update_state *state = private_data;
	TDB_DATA key = dbwrap_record_get_key(rec);

	state->status = smbXsrv_open_global_store(
		rec, key, oldval, state->global);
}

NTSTATUS smbXsrv_open_update(struct smbXsrv_open *op)
{
	struct smbXsrv_open_update_state state = { .global = op->global, };
	struct smbXsrv_open_table *table = op->table;
	struct smbXsrv_open_global_key_buf key_buf;
	TDB_DATA key = smbXsrv_open_global_id_to_key(
		op->global->open_global_id, &key_buf);
	NTSTATUS status;

	status = dbwrap_do_locked(
		table->global.db_ctx, key, smbXsrv_open_update_fn, &state);
	if (!NT_STATUS_IS_OK(status)) {
		DBG_WARNING("global_id (0x%08x) dbwrap_do_locked failed: %s\n",
			    op->global->open_global_id,
			    nt_errstr(status));
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	if (!NT_STATUS_IS_OK(state.status)) {
		DBG_WARNING("global_id (0x%08x) smbXsrv_open_global_store "
			    "failed: %s\n",
			    op->global->open_global_id,
			    nt_errstr(state.status));
		return state.status;
	}

	status = smbXsrv_open_set_replay_cache(op);
	if (!NT_STATUS_IS_OK(status)) {
		DBG_ERR("smbXsrv_open_set_replay_cache failed: %s\n",
			nt_errstr(status));
		return status;
	}

	if (CHECK_DEBUGLVL(10)) {
		struct smbXsrv_openB open_blob = {
			.version = SMBXSRV_VERSION_0,
			.info.info0 = op,
		};

		DEBUG(10,("smbXsrv_open_update: global_id (0x%08x) stored\n",
			  op->global->open_global_id));
		NDR_PRINT_DEBUG(smbXsrv_openB, &open_blob);
	}

	return NT_STATUS_OK;
}

struct smbXsrv_open_close_state {
	struct smbXsrv_open *op;
	NTSTATUS status;
};

static void smbXsrv_open_close_fn(
	struct db_record *rec, TDB_DATA oldval, void *private_data)
{
	struct smbXsrv_open_close_state *state = private_data;
	struct smbXsrv_open_global0 *global = state->op->global;
	TDB_DATA key = dbwrap_record_get_key(rec);

	if (global->durable) {
		/*
		 * Durable open -- we need to update the global part
		 * instead of deleting it
		 */
		state->status = smbXsrv_open_global_store(
			rec, key, oldval, global);
		if (!NT_STATUS_IS_OK(state->status)) {
			DBG_WARNING("failed to store global key '%s': %s\n",
				    tdb_data_dbg(key),
				    nt_errstr(state->status));
			return;
		}

		if (CHECK_DEBUGLVL(10)) {
			struct smbXsrv_openB open_blob = {
				.version = SMBXSRV_VERSION_0,
				.info.info0 = state->op,
			};

			DBG_DEBUG("(0x%08x) stored disconnect\n",
				  global->open_global_id);
			NDR_PRINT_DEBUG(smbXsrv_openB, &open_blob);
		}
		return;
	}

	state->status = dbwrap_record_delete(rec);
	if (!NT_STATUS_IS_OK(state->status)) {
		DBG_WARNING("failed to delete global key '%s': %s\n",
			    tdb_data_dbg(key),
			    nt_errstr(state->status));
	}
}

NTSTATUS smbXsrv_open_close(struct smbXsrv_open *op, NTTIME now)
{
	struct smbXsrv_open_close_state state = { .op = op, };
	struct smbXsrv_open_global0 *global = op->global;
	struct smbXsrv_open_table *table;
	NTSTATUS status;
	NTSTATUS error = NT_STATUS_OK;
	struct smbXsrv_open_global_key_buf key_buf;
	TDB_DATA key = smbXsrv_open_global_id_to_key(
		global->open_global_id, &key_buf);
	int ret;

	error = smbXsrv_open_clear_replay_cache(op);
	if (!NT_STATUS_IS_OK(error)) {
		DBG_ERR("smbXsrv_open_clear_replay_cache failed: %s\n",
			nt_errstr(error));
	}

	if (op->table == NULL) {
		return error;
	}

	table = op->table;
	op->table = NULL;

	op->status = NT_STATUS_FILE_CLOSED;
	global->disconnect_time = now;
	server_id_set_disconnected(&global->server_id);

	status = dbwrap_do_locked(
		table->global.db_ctx, key, smbXsrv_open_close_fn, &state);
	if (!NT_STATUS_IS_OK(status)) {
		DBG_WARNING("dbwrap_do_locked() for %s failed: %s\n",
			    tdb_data_dbg(key),
			    nt_errstr(status));
		error = status;
	} else if (!NT_STATUS_IS_OK(state.status)) {
		DBG_WARNING("smbXsrv_open_close_fn() for %s failed: %s\n",
			    tdb_data_dbg(key),
			    nt_errstr(state.status));
		error = state.status;
	}

	ret = idr_remove(table->local.idr, op->local_id);
	SMB_ASSERT(ret == 0);

	table->local.num_opens -= 1;

	if (op->compat) {
		op->compat->op = NULL;
		file_free(NULL, op->compat);
		op->compat = NULL;
	}

	return error;
}

NTSTATUS smb1srv_open_table_init(struct smbXsrv_connection *conn)
{
	uint32_t max_opens;

	/*
	 * Allow a range from 1..65534.
	 *
	 * With real_max_open_files possible ids,
	 * truncated to the SMB1 limit of 16-bit.
	 *
	 * 0 and 0xFFFF are no valid ids.
	 */
	max_opens = conn->client->sconn->real_max_open_files;
	max_opens = MIN(max_opens, UINT16_MAX - 1);

	return smbXsrv_open_table_init(conn, 1, UINT16_MAX - 1, max_opens);
}

NTSTATUS smb1srv_open_lookup(struct smbXsrv_connection *conn,
			     uint16_t fnum, NTTIME now,
			     struct smbXsrv_open **_open)
{
	struct smbXsrv_open_table *table = conn->client->open_table;
	uint32_t local_id = fnum;
	uint32_t global_id = 0;

	return smbXsrv_open_local_lookup(table, local_id, global_id, now, _open);
}

NTSTATUS smb2srv_open_table_init(struct smbXsrv_connection *conn)
{
	uint32_t max_opens;
	uint32_t highest_id;

	/*
	 * Allow a range from 1..4294967294.
	 *
	 * With real_max_open_files possible ids,
	 * truncated to 16-bit (the same as SMB1 for now).
	 *
	 * 0 and 0xFFFFFFFF are no valid ids.
	 *
	 * The usage of conn->sconn->real_max_open_files
	 * is the reason that we use one open table per
	 * transport connection (as we still have a 1:1 mapping
	 * between process and transport connection).
	 */
	max_opens = conn->client->sconn->real_max_open_files;
	max_opens = MIN(max_opens, UINT16_MAX - 1);

	/*
	 * idtree uses "int" for local IDs. Limit the maximum ID to
	 * what "int" can hold.
	 */
	highest_id = UINT32_MAX-1;
	highest_id = MIN(highest_id, INT_MAX);

	return smbXsrv_open_table_init(conn, 1, highest_id, max_opens);
}

NTSTATUS smb2srv_open_lookup(struct smbXsrv_connection *conn,
			     uint64_t persistent_id,
			     uint64_t volatile_id,
			     NTTIME now,
			     struct smbXsrv_open **_open)
{
	struct smbXsrv_open_table *table = conn->client->open_table;
	uint32_t local_id = volatile_id & UINT32_MAX;
	uint64_t local_zeros = volatile_id & 0xFFFFFFFF00000000LLU;
	uint32_t global_id = persistent_id & UINT32_MAX;
	uint64_t global_zeros = persistent_id & 0xFFFFFFFF00000000LLU;
	NTSTATUS status;

	if (local_zeros != 0) {
		return NT_STATUS_FILE_CLOSED;
	}

	if (global_zeros != 0) {
		return NT_STATUS_FILE_CLOSED;
	}

	if (global_id == 0) {
		return NT_STATUS_FILE_CLOSED;
	}

	status = smbXsrv_open_local_lookup(table, local_id, global_id, now,
					   _open);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/*
	 * Clear the replay cache for this create_guid if it exists:
	 * This is based on the assumption that this lookup will be
	 * triggered by a client request using the file-id for lookup.
	 * Hence the client has proven that it has in fact seen the
	 * reply to its initial create call. So subsequent create replays
	 * should be treated as invalid. Hence the index for create_guid
	 * lookup needs to be removed.
	 */
	status = smbXsrv_open_clear_replay_cache(*_open);

	return status;
}

/*
 * This checks or marks the replay cache, we have the following
 * cases:
 *
 * 1. There is no record in the cache
 *    => we add the passes caller_req_guid as holder_req_guid
 *       together with local_id as 0.
 *    => We return STATUS_FWP_RESERVED in order to indicate
 *       that the caller holds the current reservation
 *
 * 2. There is a record in the cache and holder_req_guid
 *    is already the same as caller_req_guid and local_id is 0
 *    => We return STATUS_FWP_RESERVED in order to indicate
 *       that the caller holds the current reservation
 *
 * 3. There is a record in the cache with a holder_req_guid
 *    other than caller_req_guid (and local_id is 0):
 *    => We return NT_STATUS_FILE_NOT_AVAILABLE to indicate
 *       the original request is still pending
 *
 * 4. There is a record in the cache with a zero holder_req_guid
 *    and a valid local_id:
 *    => We lookup the existing open by local_id
 *    => We return NT_STATUS_OK together with the smbXsrv_open
 *
 *
 * With NT_STATUS_OK the caller can continue the replay processing.
 *
 * With STATUS_FWP_RESERVED the caller should continue the normal
 * open processing:
 * - On success:
 *   - smbXsrv_open_update()/smbXsrv_open_set_replay_cache()
 *     will convert the record to a zero holder_req_guid
 *     with a valid local_id.
 * - On failure:
 *   - smbXsrv_open_purge_replay_cache() should cleanup
 *     the reservation.
 *
 * All other values should be returned to the client,
 * while NT_STATUS_FILE_NOT_AVAILABLE will trigger the
 * retry loop on the client.
 */
NTSTATUS smb2srv_open_lookup_replay_cache(struct smbXsrv_connection *conn,
					  struct GUID caller_req_guid,
					  struct GUID create_guid,
					  const char *name,
					  NTTIME now,
					  struct smbXsrv_open **_open)
{
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS status;
	struct smbXsrv_open_table *table = conn->client->open_table;
	struct db_context *db = table->local.replay_cache_db_ctx;
	struct GUID_txt_buf tmp_guid_buf;
	struct GUID_txt_buf _create_guid_buf;
	const char *create_guid_str = GUID_buf_string(&create_guid, &_create_guid_buf);
	TDB_DATA create_guid_key = string_term_tdb_data(create_guid_str);
	struct db_record *db_rec = NULL;
	struct smbXsrv_open *op = NULL;
	struct smbXsrv_open_replay_cache rc = {
		.holder_req_guid = caller_req_guid,
		.idle_time = now,
		.local_id = 0,
	};
	enum ndr_err_code ndr_err;
	DATA_BLOB blob = data_blob_null;
	TDB_DATA val;

	*_open = NULL;

	db_rec = dbwrap_fetch_locked(db, frame, create_guid_key);
	if (db_rec == NULL) {
		TALLOC_FREE(frame);
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	val = dbwrap_record_get_value(db_rec);
	if (val.dsize == 0) {
		uint8_t data[SMBXSRV_OPEN_REPLAY_CACHE_FIXED_SIZE];

		blob = data_blob_const(data, ARRAY_SIZE(data));
		ndr_err = ndr_push_struct_into_fixed_blob(&blob, &rc,
			(ndr_push_flags_fn_t)ndr_push_smbXsrv_open_replay_cache);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			TALLOC_FREE(frame);
			return status;
		}

		val = make_tdb_data(blob.data, blob.length);
		status = dbwrap_record_store(db_rec, val, TDB_REPLACE);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(frame);
			return status;
		}

		/*
		 * We're the new holder
		 */
		*_open = NULL;
		TALLOC_FREE(frame);
		return NT_STATUS_FWP_RESERVED;
	}

	if (val.dsize != SMBXSRV_OPEN_REPLAY_CACHE_FIXED_SIZE) {
		TALLOC_FREE(frame);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	blob = data_blob_const(val.dptr, val.dsize);
	ndr_err = ndr_pull_struct_blob_all_noalloc(&blob, &rc,
			(ndr_pull_flags_fn_t)ndr_pull_smbXsrv_open_replay_cache);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		TALLOC_FREE(frame);
		return status;
	}
	if (rc.local_id != 0) {
		if (GUID_equal(&rc.holder_req_guid, &caller_req_guid)) {
			/*
			 * This should not happen
			 */
			status = NT_STATUS_INTERNAL_ERROR;
			DBG_ERR("caller %s already holds local_id %u for create %s [%s] - %s\n",
				GUID_buf_string(&caller_req_guid, &tmp_guid_buf),
				(unsigned)rc.local_id,
				create_guid_str,
				name,
				nt_errstr(status));

			TALLOC_FREE(frame);
			return status;
		}

		status = smbXsrv_open_local_lookup(table,
						   rc.local_id,
						   0, /* global_id */
						   now,
						   &op);
		if (!NT_STATUS_IS_OK(status)) {
			DBG_ERR("holder %s stale for local_id %u for create %s [%s] - %s\n",
				GUID_buf_string(&rc.holder_req_guid, &tmp_guid_buf),
				(unsigned)rc.local_id,
				create_guid_str,
				name,
				nt_errstr(status));

			TALLOC_FREE(frame);
			return status;
		}

		/*
		 * We found an open the caller can reuse.
		 */
		SMB_ASSERT(op != NULL);
		*_open = op;
		TALLOC_FREE(frame);
		return NT_STATUS_OK;
	}

	if (GUID_equal(&rc.holder_req_guid, &caller_req_guid)) {
		/*
		 * We're still the holder
		 */
		*_open = NULL;
		TALLOC_FREE(frame);
		return NT_STATUS_FWP_RESERVED;
	}

	/*
	 * The original request (or a former replay) is still
	 * pending, ask the client to retry by sending FILE_NOT_AVAILABLE.
	 */
	status = NT_STATUS_FILE_NOT_AVAILABLE;
	DBG_DEBUG("holder %s still pending for create %s [%s] - %s\n",
		   GUID_buf_string(&rc.holder_req_guid, &tmp_guid_buf),
		   create_guid_str,
		   name,
		   nt_errstr(status));
	TALLOC_FREE(frame);
	return status;
}

struct smb2srv_open_recreate_state {
	struct smbXsrv_open *op;
	const struct GUID *create_guid;
	struct security_token *current_token;
	struct server_id me;

	NTSTATUS status;
};

static void smb2srv_open_recreate_fn(
	struct db_record *rec, TDB_DATA oldval, void *private_data)
{
	struct smb2srv_open_recreate_state *state = private_data;
	TDB_DATA key = dbwrap_record_get_key(rec);
	struct smbXsrv_open_global0 *global = NULL;

	state->status = smbXsrv_open_global_verify_record(
		key, oldval, state->op, &state->op->global);
	if (!NT_STATUS_IS_OK(state->status)) {
		DBG_WARNING("smbXsrv_open_global_verify_record for %s "
			    "failed: %s\n",
			    tdb_data_dbg(key),
			    nt_errstr(state->status));
		goto not_found;
	}
	global = state->op->global;

	/*
	 * If the provided create_guid is NULL, this means that
	 * the reconnect request was a v1 request. In that case
	 * we should skip the create GUID verification, since
	 * it is valid to v1-reconnect a v2-opened handle.
	 */
	if ((state->create_guid != NULL) &&
	    !GUID_equal(&global->create_guid, state->create_guid)) {
		struct GUID_txt_buf buf1, buf2;
		DBG_NOTICE("%s != %s in %s\n",
			   GUID_buf_string(&global->create_guid, &buf1),
			   GUID_buf_string(state->create_guid, &buf2),
			   tdb_data_dbg(key));
		goto not_found;
	}

	if (!security_token_is_sid(
		    state->current_token, &global->open_owner)) {
		struct dom_sid_buf buf;
		DBG_NOTICE("global owner %s not in our token in %s\n",
			   dom_sid_str_buf(&global->open_owner, &buf),
			   tdb_data_dbg(key));
		goto not_found;
	}

	if (!global->durable) {
		DBG_NOTICE("%"PRIu64"/%"PRIu64" not durable in %s\n",
			   global->open_persistent_id,
			   global->open_volatile_id,
			   tdb_data_dbg(key));
		goto not_found;
	}

	global->open_volatile_id = state->op->local_id;
	global->server_id = state->me;

	state->status = smbXsrv_open_global_store(rec, key, oldval, global);
	if (!NT_STATUS_IS_OK(state->status)) {
		DBG_WARNING("smbXsrv_open_global_store for %s failed: %s\n",
			    tdb_data_dbg(key),
			    nt_errstr(state->status));
		return;
	}
	return;

not_found:
	state->status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

NTSTATUS smb2srv_open_recreate(struct smbXsrv_connection *conn,
			       struct auth_session_info *session_info,
			       uint64_t persistent_id,
			       const struct GUID *create_guid,
			       NTTIME now,
			       struct smbXsrv_open **_open)
{
	struct smbXsrv_open_table *table = conn->client->open_table;
	struct smb2srv_open_recreate_state state = {
		.create_guid = create_guid,
		.me = messaging_server_id(conn->client->msg_ctx),
	};
	struct smbXsrv_open_global_key_buf key_buf;
	TDB_DATA key = smbXsrv_open_global_id_to_key(
		persistent_id & UINT32_MAX, &key_buf);
	int ret, local_id;
	NTSTATUS status;

	if (session_info == NULL) {
		DEBUG(10, ("session_info=NULL\n"));
		return NT_STATUS_INVALID_HANDLE;
	}
	state.current_token = session_info->security_token;

	if (state.current_token == NULL) {
		DEBUG(10, ("current_token=NULL\n"));
		return NT_STATUS_INVALID_HANDLE;
	}

	if ((persistent_id & 0xFFFFFFFF00000000LLU) != 0) {
		/*
		 * We only use 32 bit for the persistent ID
		 */
		DBG_DEBUG("persistent_id=%"PRIx64"\n", persistent_id);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (table->local.num_opens >= table->local.max_opens) {
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}

	state.op = talloc_zero(table, struct smbXsrv_open);
	if (state.op == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state.op->table = table;

	local_id =  idr_get_new_random(
		table->local.idr,
		state.op,
		table->local.lowest_id,
		table->local.highest_id);
	if (local_id == -1) {
		TALLOC_FREE(state.op);
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}
	state.op->local_id = local_id;
	SMB_ASSERT(state.op->local_id == local_id); /* No coercion loss */

	table->local.num_opens += 1;

	state.op->idle_time = now;
	state.op->status = NT_STATUS_FILE_CLOSED;

	status = dbwrap_do_locked(
		table->global.db_ctx, key, smb2srv_open_recreate_fn, &state);
	if (!NT_STATUS_IS_OK(status)) {
		DBG_DEBUG("dbwrap_do_locked() for %s failed: %s\n",
			  tdb_data_dbg(key),
			  nt_errstr(status));
		goto fail;
	}

	if (!NT_STATUS_IS_OK(state.status)) {
		status = state.status;
		DBG_DEBUG("smb2srv_open_recreate_fn for %s failed: %s\n",
			  tdb_data_dbg(key),
			  nt_errstr(status));
		goto fail;
	}

	talloc_set_destructor(state.op, smbXsrv_open_destructor);

	if (CHECK_DEBUGLVL(10)) {
		struct smbXsrv_openB open_blob = {
			.info.info0 = state.op,
		};
		DBG_DEBUG("global_id (0x%08x) stored\n",
			  state.op->global->open_global_id);
		NDR_PRINT_DEBUG(smbXsrv_openB, &open_blob);
	}

	*_open = state.op;

	return NT_STATUS_OK;
fail:
	table->local.num_opens -= 1;

	ret = idr_remove(table->local.idr, state.op->local_id);
	SMB_ASSERT(ret == 0);
	TALLOC_FREE(state.op);
	return status;
}

struct smbXsrv_open_global_traverse_state {
	int (*fn)(struct db_record *rec, struct smbXsrv_open_global0 *, void *);
	void *private_data;
};

static int smbXsrv_open_global_traverse_fn(struct db_record *rec, void *data)
{
	struct smbXsrv_open_global_traverse_state *state =
		(struct smbXsrv_open_global_traverse_state*)data;
	struct smbXsrv_open_global0 *global = NULL;
	TDB_DATA key = dbwrap_record_get_key(rec);
	TDB_DATA val = dbwrap_record_get_value(rec);
	NTSTATUS status;
	int ret = -1;

	status = smbXsrv_open_global_parse_record(
		talloc_tos(), key, val, &global);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	ret = state->fn(rec, global, state->private_data);
	talloc_free(global);
	return ret;
}

NTSTATUS smbXsrv_open_global_traverse(
	int (*fn)(struct db_record *rec, struct smbXsrv_open_global0 *, void *),
	void *private_data)
{

	NTSTATUS status;
	int count = 0;
	struct smbXsrv_open_global_traverse_state state = {
		.fn = fn,
		.private_data = private_data,
	};

	become_root();
	status = smbXsrv_open_global_init();
	if (!NT_STATUS_IS_OK(status)) {
		unbecome_root();
		DEBUG(0, ("Failed to initialize open_global: %s\n",
			  nt_errstr(status)));
		return status;
	}

	status = dbwrap_traverse_read(smbXsrv_open_global_db_ctx,
				      smbXsrv_open_global_traverse_fn,
				      &state,
				      &count);
	unbecome_root();

	return status;
}

struct smbXsrv_open_cleanup_state {
	uint32_t global_id;
	NTSTATUS status;
};

static void smbXsrv_open_cleanup_fn(
	struct db_record *rec, TDB_DATA oldval, void *private_data)
{
	struct smbXsrv_open_cleanup_state *state = private_data;
	struct smbXsrv_open_global0 *global = NULL;
	TDB_DATA key = dbwrap_record_get_key(rec);
	bool delete_open = false;

	if (oldval.dsize == 0) {
		DBG_DEBUG("[global: 0x%08x] "
			  "empty record in %s, skipping...\n",
			  state->global_id,
			  dbwrap_name(dbwrap_record_get_db(rec)));
		state->status = NT_STATUS_OK;
		return;
	}

	state->status = smbXsrv_open_global_parse_record(
		talloc_tos(), key, oldval, &global);
	if (!NT_STATUS_IS_OK(state->status)) {
		DBG_WARNING("[global: %x08x] "
			    "smbXsrv_open_global_parse_record() in %s "
			    "failed: %s, deleting record\n",
			    state->global_id,
			    dbwrap_name(dbwrap_record_get_db(rec)),
			    nt_errstr(state->status));
		delete_open = true;
		goto do_delete;
	}

	if (server_id_is_disconnected(&global->server_id)) {
		struct timeval now = timeval_current();
		struct timeval disconnect_time;
		struct timeval_buf buf;
		int64_t tdiff;

		nttime_to_timeval(&disconnect_time, global->disconnect_time);
		tdiff = usec_time_diff(&now, &disconnect_time);
		delete_open = (tdiff >= 1000*global->durable_timeout_msec);

		DBG_DEBUG("[global: 0x%08x] "
			  "disconnected at [%s] %"PRIi64"s ago with "
			  "timeout of %"PRIu32"s -%s reached\n",
			  state->global_id,
			  timeval_str_buf(&disconnect_time,
					  false,
					  false,
					  &buf),
			  tdiff/1000000,
			  global->durable_timeout_msec / 1000,
			  delete_open ? "" : " not");
	} else if (!serverid_exists(&global->server_id)) {
		struct server_id_buf idbuf;
		DBG_DEBUG("[global: 0x%08x] "
			  "server[%s] does not exist\n",
			  state->global_id,
			  server_id_str_buf(global->server_id, &idbuf));
		delete_open = true;
	}

	if (!delete_open) {
		state->status = NT_STATUS_OK;
		return;
	}
do_delete:
	state->status = dbwrap_record_delete(rec);
	if (!NT_STATUS_IS_OK(state->status)) {
		DBG_WARNING("[global: 0x%08x] "
			    "failed to delete record"
			    "from %s: %s\n",
			    state->global_id,
			    dbwrap_name(dbwrap_record_get_db(rec)),
			    nt_errstr(state->status));
		return;
	}

	DBG_DEBUG("[global: 0x%08x] "
		  "deleted record from %s\n",
		  state->global_id,
		  dbwrap_name(dbwrap_record_get_db(rec)));
}

NTSTATUS smbXsrv_open_cleanup(uint64_t persistent_id)
{
	struct smbXsrv_open_cleanup_state state = {
		.global_id = persistent_id & UINT32_MAX,
	};
	struct smbXsrv_open_global_key_buf key_buf;
	TDB_DATA key = smbXsrv_open_global_id_to_key(
		state.global_id, &key_buf);
	NTSTATUS status;

	status = dbwrap_do_locked(
		smbXsrv_open_global_db_ctx,
		key,
		smbXsrv_open_cleanup_fn,
		&state);
	if (!NT_STATUS_IS_OK(status)) {
		DBG_DEBUG("[global: 0x%08x] dbwrap_do_locked failed: %s\n",
			  state.global_id,
			  nt_errstr(status));
		return status;
	}

	return state.status;
}
