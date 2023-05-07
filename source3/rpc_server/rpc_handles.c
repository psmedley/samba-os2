/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Jeremy Allison			   2001.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "system/passwd.h" /* uid_wrapper */
#include "../librpc/gen_ndr/ndr_lsa.h"
#include "../librpc/gen_ndr/ndr_samr.h"
#include "auth.h"
#include "rpc_server/rpc_pipes.h"
#include "../libcli/security/security.h"
#include "lib/tsocket/tsocket.h"
#include "librpc/ndr/ndr_table.h"
#include "librpc/rpc/dcesrv_core.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

static size_t num_handles = 0;

bool check_open_pipes(void)
{
	if (num_handles > 0) {
		return true;
	}

	return false;
}

size_t num_pipe_handles(void)
{
       return num_handles;
}

/****************************************************************************
  find first available policy slot.  creates a policy handle for you.

  If "data_ptr" is given, this must be a talloc'ed object, create_policy_hnd
  talloc_moves this into the handle. If the policy_hnd is closed,
  data_ptr is TALLOC_FREE()'ed
****************************************************************************/

struct hnd_cnt {
	bool _dummy;
};

static int hnd_cnt_destructor(struct hnd_cnt *cnt)
{
	num_handles--;
	return 0;
}

bool create_policy_hnd(struct pipes_struct *p,
		       struct policy_handle *hnd,
		       uint8_t handle_type,
		       void *data_ptr)
{
	struct dcesrv_handle *rpc_hnd = NULL;
	struct hnd_cnt *cnt = NULL;

	rpc_hnd = dcesrv_handle_create(p->dce_call, handle_type);
	if (rpc_hnd == NULL) {
		return false;
	}

	cnt = talloc_zero(rpc_hnd, struct hnd_cnt);
	if (cnt == NULL) {
		TALLOC_FREE(rpc_hnd);
		return false;
	}
	talloc_set_destructor(cnt, hnd_cnt_destructor);

	if (data_ptr != NULL) {
		rpc_hnd->data = talloc_move(rpc_hnd, &data_ptr);
	}

	*hnd = rpc_hnd->wire_handle;

	num_handles++;

	return true;
}

/****************************************************************************
  find policy by handle - internal version.
****************************************************************************/

static struct dcesrv_handle *find_policy_by_hnd_internal(
					struct pipes_struct *p,
					const struct policy_handle *hnd,
					uint8_t handle_type,
					void **data_p)
{
	struct dcesrv_handle *h = NULL;

	if (data_p) {
		*data_p = NULL;
	}

	/*
	 * Do not pass an empty policy_handle to dcesrv_handle_lookup() or
	 * it will create a new empty handle
	 */
	if (ndr_policy_handle_empty(hnd)) {
		p->fault_state = DCERPC_FAULT_CONTEXT_MISMATCH;
		return NULL;
	}

	/*
	 * Do not pass handle_type to avoid setting the fault_state in the
	 * pipes_struct if the handle type does not match
	 */
	h = dcesrv_handle_lookup(p->dce_call, hnd, DCESRV_HANDLE_ANY);
	if (h == NULL) {
		p->fault_state = DCERPC_FAULT_CONTEXT_MISMATCH;
		return NULL;
	}

	if (handle_type != DCESRV_HANDLE_ANY &&
	    h->wire_handle.handle_type != handle_type) {
		/* Just return NULL, do not set a fault
		 * state in pipes_struct */
		return NULL;
	}

	if (data_p) {
		*data_p = h->data;
	}

	return h;
}

/****************************************************************************
  find policy by handle
****************************************************************************/

void *_find_policy_by_hnd(struct pipes_struct *p,
			  const struct policy_handle *hnd,
			  uint8_t handle_type,
			  NTSTATUS *pstatus)
{
	struct dcesrv_handle *rpc_hnd = NULL;
	void *data = NULL;

	rpc_hnd = find_policy_by_hnd_internal(p, hnd, handle_type, &data);
	if (rpc_hnd == NULL) {
		*pstatus = NT_STATUS_INVALID_HANDLE;
		return NULL;
	}

	*pstatus = NT_STATUS_OK;
	return data;
}

/****************************************************************************
  Close a policy.
****************************************************************************/

bool close_policy_hnd(struct pipes_struct *p,
		      struct policy_handle *hnd)
{
	struct dcesrv_handle *rpc_hnd = NULL;

	rpc_hnd = find_policy_by_hnd_internal(p, hnd, DCESRV_HANDLE_ANY, NULL);
	if (rpc_hnd == NULL) {
		DEBUG(3, ("Error closing policy (policy not found)\n"));
		return false;
	}

	TALLOC_FREE(rpc_hnd);

	return true;
}

/*******************************************************************
Shall we allow access to this rpc?  Currently this function
implements the 'restrict anonymous' setting by denying access to
anonymous users if the restrict anonymous level is > 0.  Further work
will be checking a security descriptor to determine whether a user
token has enough access to access the pipe.
********************************************************************/

bool pipe_access_check(struct pipes_struct *p)
{
	/* Don't let anonymous users access this RPC if restrict
	   anonymous > 0 */

	if (lp_restrict_anonymous() > 0) {

		struct dcesrv_call_state *dce_call = p->dce_call;
		struct dcesrv_auth *auth_state = dce_call->auth_state;
		enum dcerpc_AuthType auth_type = DCERPC_AUTH_TYPE_NONE;
		struct auth_session_info *session_info = NULL;
		enum security_user_level user_level;

		if (!auth_state->auth_finished) {
			return false;
		}

		dcesrv_call_auth_info(dce_call, &auth_type, NULL);

		/* schannel, so we must be ok */
		if (auth_type == DCERPC_AUTH_TYPE_SCHANNEL) {
			return True;
		}

		session_info = dcesrv_call_session_info(dce_call);
		user_level = security_session_user_level(session_info, NULL);

		if (user_level < SECURITY_USER) {
			return False;
		}
	}

	return True;
}
