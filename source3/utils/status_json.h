/*
 * Samba Unix/Linux SMB client library
 * Json output
 * Copyright (C) Jule Anger 2022
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "status.h"
#include "smbd/notifyd/notifyd_db.h"

#ifndef STATUS_JSON_H
#define STATUS_JSON_H

int add_section_to_json(struct traverse_state *state,
			const char *key);

int add_general_information_to_json(struct traverse_state *state);

int add_profile_item_to_json(struct traverse_state *state,
			     const char *section,
			     const char *subsection,
			     const char *key,
			     uintmax_t value);

int traverse_connections_json(struct traverse_state *state,
			      const struct connections_data *crec,
			      const char *encryption_cipher,
			      enum crypto_degree encryption_degree,
			      const char *signing_cipher,
			      enum crypto_degree signing_degree);

int traverse_sessionid_json(struct traverse_state *state,
			    struct sessionid *session,
			    char *uid_str,
			    char *gid_str,
			    const char *encryption_cipher,
			    enum crypto_degree encryption_degree,
			    const char *signing_cipher,
			    enum crypto_degree signing_degree,
			    const char *connection_dialect);

int print_share_mode_json(struct traverse_state *state,
			  const struct share_mode_data *d,
			  const struct share_mode_entry *e,
			  struct file_id fid,
			  const char *uid_str,
			  const char *op_str,
			  uint32_t lease_type,
			  const char *filename);

int print_brl_json(struct traverse_state *state,
		   const struct server_id server_id,
		   struct file_id fid,
		   const char *type,
		   enum brl_flavour flavour,
		   intmax_t start,
		   intmax_t size,
		   const char *sharepath,
		   const char *filename);

bool print_notify_rec_json(struct traverse_state *state,
			   const struct notify_instance *instance,
			   const struct server_id server_id,
			   const char *path);
#endif
