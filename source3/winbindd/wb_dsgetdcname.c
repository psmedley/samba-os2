/*
   Unix SMB/CIFS implementation.
   async dsgetdcname
   Copyright (C) Volker Lendecke 2009

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
#include "winbindd.h"
#include "librpc/gen_ndr/ndr_winbind_c.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "lib/gencache.h"

struct wb_dsgetdcname_state {
	const char *domain_name;
	struct GUID domain_guid;
	struct netr_DsRGetDCNameInfo *dcinfo;
};

static void wb_dsgetdcname_done(struct tevent_req *subreq);

struct tevent_req *wb_dsgetdcname_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       const char *domain_name,
				       const struct GUID *domain_guid,
				       const char *site_name,
				       uint32_t flags)
{
	struct tevent_req *req, *subreq;
	struct wb_dsgetdcname_state *state;
	struct dcerpc_binding_handle *child_binding_handle = NULL;
	struct GUID *guid_ptr = NULL;

	req = tevent_req_create(mem_ctx, &state, struct wb_dsgetdcname_state);
	if (req == NULL) {
		return NULL;
	}

	D_INFO("WB command dsgetdcname start.\n"
	       "Search domain name %s and site name %s.\n",
	       domain_name,
	       site_name);
	if (strequal(domain_name, "BUILTIN")) {
		/*
		 * This makes no sense
		 */
		tevent_req_nterror(req, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND);
		return tevent_req_post(req, ev);
	}

	if (strequal(domain_name, get_global_sam_name())) {
		int role = lp_server_role();
		if ( role != ROLE_ACTIVE_DIRECTORY_DC ) {
			/*
			 * Two options here: Give back our own address, or say there's
			 * nobody around. Right now opting for the latter, one measure
			 * to prevent the loopback connects. This might change if
			 * needed.
			 */
			tevent_req_nterror(req, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND);
			return tevent_req_post(req, ev);
		}
	}

	if (IS_DC) {
		/*
		 * We have to figure out the DC ourselves
		 */
		child_binding_handle = locator_child_handle();
	} else {
		struct winbindd_domain *domain = find_our_domain();
		child_binding_handle = dom_child_handle(domain);
	}

	if (domain_guid != NULL) {
		/* work around a const issue in rpccli_ autogenerated code */
		state->domain_guid = *domain_guid;
		guid_ptr = &state->domain_guid;
	}

	state->domain_name = talloc_strdup(state, domain_name);
	if (tevent_req_nomem(state->domain_name, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = dcerpc_wbint_DsGetDcName_send(
		state, ev, child_binding_handle, domain_name, guid_ptr, site_name,
		flags, &state->dcinfo);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_dsgetdcname_done, req);
	return req;
}

static void wb_dsgetdcname_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_dsgetdcname_state *state = tevent_req_data(
		req, struct wb_dsgetdcname_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_DsGetDcName_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS wb_dsgetdcname_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			     struct netr_DsRGetDCNameInfo **pdcinfo)
{
	struct wb_dsgetdcname_state *state = tevent_req_data(
		req, struct wb_dsgetdcname_state);
	NTSTATUS status;

	D_INFO("WB command dsgetdcname for %s end.\n",
	       state->domain_name);
	if (tevent_req_is_nterror(req, &status)) {
		D_NOTICE("Failed for %s with %s.\n",
			 state->domain_name,
			 nt_errstr(status));
		return status;
	}
	*pdcinfo = talloc_move(mem_ctx, &state->dcinfo);
	return NT_STATUS_OK;
}

NTSTATUS wb_dsgetdcname_gencache_set(const char *domname,
				     struct netr_DsRGetDCNameInfo *dcinfo)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	char *key;
	bool ok;

	key = talloc_asprintf_strupper_m(talloc_tos(), "DCINFO/%s", domname);
	if (key == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (DEBUGLEVEL >= DBGLVL_DEBUG) {
		NDR_PRINT_DEBUG(netr_DsRGetDCNameInfo, dcinfo);
	}

	ndr_err = ndr_push_struct_blob(
		&blob, key, dcinfo,
		(ndr_push_flags_fn_t)ndr_push_netr_DsRGetDCNameInfo);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		DBG_WARNING("ndr_push_struct_blob failed: %s\n",
			    ndr_errstr(ndr_err));
		TALLOC_FREE(key);
		return status;
	}

	ok = gencache_set_data_blob(key, blob, time(NULL)+3600);

	if (!ok) {
		DBG_WARNING("gencache_set_data_blob for key %s failed\n", key);
		TALLOC_FREE(key);
		return NT_STATUS_UNSUCCESSFUL;
	}

	TALLOC_FREE(key);
	return NT_STATUS_OK;
}

struct dcinfo_parser_state {
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct netr_DsRGetDCNameInfo *dcinfo;
};

static void dcinfo_parser(const struct gencache_timeout *timeout,
			  DATA_BLOB blob,
			  void *private_data)
{
	struct dcinfo_parser_state *state = private_data;
	enum ndr_err_code ndr_err;

	if (gencache_timeout_expired(timeout)) {
		return;
	}

	state->dcinfo = talloc(state->mem_ctx, struct netr_DsRGetDCNameInfo);
	if (state->dcinfo == NULL) {
		state->status = NT_STATUS_NO_MEMORY;
		return;
	}

	ndr_err = ndr_pull_struct_blob_all(
		&blob, state->dcinfo, state->dcinfo,
		(ndr_pull_flags_fn_t)ndr_pull_netr_DsRGetDCNameInfo);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DBG_ERR("ndr_pull_struct_blob failed\n");
		state->status = ndr_map_error2ntstatus(ndr_err);
		TALLOC_FREE(state->dcinfo);
		return;
	}

	state->status = NT_STATUS_OK;
}

NTSTATUS wb_dsgetdcname_gencache_get(TALLOC_CTX *mem_ctx,
				     const char *domname,
				     struct netr_DsRGetDCNameInfo **dcinfo)
{
	struct dcinfo_parser_state state;
	char *key;
	bool ok;

	key = talloc_asprintf_strupper_m(mem_ctx, "DCINFO/%s", domname);
	if (key == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	state = (struct dcinfo_parser_state) {
		.status = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND,
		.mem_ctx = mem_ctx,
	};

	ok = gencache_parse(key, dcinfo_parser, &state);
	TALLOC_FREE(key);
	if (!ok) {
		return NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	}

	if (!NT_STATUS_IS_OK(state.status)) {
		return state.status;
	}

	if (DEBUGLEVEL >= DBGLVL_DEBUG) {
		NDR_PRINT_DEBUG(netr_DsRGetDCNameInfo, state.dcinfo);
	}

	*dcinfo = state.dcinfo;
	return NT_STATUS_OK;
}
