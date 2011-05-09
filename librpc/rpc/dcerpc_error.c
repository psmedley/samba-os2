/*
   Unix SMB/CIFS implementation.

   dcerpc fault functions

   Copyright (C) Stefan Metzmacher 2004

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
#include "librpc/rpc/dcerpc.h"
#include "rpc_common.h"

struct dcerpc_fault_table {
	const char *errstr;
	uint32_t faultcode;
};

static const struct dcerpc_fault_table dcerpc_faults[] =
{
#define _FAULT_STR(x) { #x , x }
	_FAULT_STR(DCERPC_NCA_S_COMM_FAILURE),
	_FAULT_STR(DCERPC_NCA_S_OP_RNG_ERROR),
	_FAULT_STR(DCERPC_NCA_S_UNKNOWN_IF),
	_FAULT_STR(DCERPC_NCA_S_WRONG_BOOT_TIME),
	_FAULT_STR(DCERPC_NCA_S_YOU_CRASHED),
	_FAULT_STR(DCERPC_NCA_S_PROTO_ERROR),
	_FAULT_STR(DCERPC_NCA_S_OUT_ARGS_TOO_BIG),
	_FAULT_STR(DCERPC_NCA_S_SERVER_TOO_BUSY),
	_FAULT_STR(DCERPC_NCA_S_FAULT_STRING_TOO_LARGE),
	_FAULT_STR(DCERPC_NCA_S_UNSUPPORTED_TYPE),
	_FAULT_STR(DCERPC_NCA_S_FAULT_INT_DIV_BY_ZERO),
	_FAULT_STR(DCERPC_NCA_S_FAULT_ADDR_ERROR),
	_FAULT_STR(DCERPC_NCA_S_FAULT_FP_DIV_BY_ZERO),
	_FAULT_STR(DCERPC_NCA_S_FAULT_FP_UNDERFLOW),
	_FAULT_STR(DCERPC_NCA_S_FAULT_FP_OVERRFLOW),
	_FAULT_STR(DCERPC_NCA_S_FAULT_INVALID_TAG),
	_FAULT_STR(DCERPC_NCA_S_FAULT_INVALID_BOUND),
	_FAULT_STR(DCERPC_NCA_S_FAULT_RPC_VERSION_MISMATCH),
	_FAULT_STR(DCERPC_NCA_S_FAULT_UNSPEC_REJECT),
	_FAULT_STR(DCERPC_NCA_S_FAULT_BAD_ACTID),
	_FAULT_STR(DCERPC_NCA_S_FAULT_WHO_ARE_YOU_FAILED),
	_FAULT_STR(DCERPC_NCA_S_FAULT_MANAGER_NOT_ENTERED),
	_FAULT_STR(DCERPC_NCA_S_FAULT_CANCEL),
	_FAULT_STR(DCERPC_NCA_S_FAULT_ILL_INST),
	_FAULT_STR(DCERPC_NCA_S_FAULT_FP_ERROR),
	_FAULT_STR(DCERPC_NCA_S_FAULT_INT_OVERFLOW),
	_FAULT_STR(DCERPC_NCA_S_UNUSED_1C000011),
	_FAULT_STR(DCERPC_NCA_S_FAULT_UNSPEC),
	_FAULT_STR(DCERPC_NCA_S_FAULT_REMOTE_COMM_FAILURE),
	_FAULT_STR(DCERPC_NCA_S_FAULT_PIPE_EMPTY),
	_FAULT_STR(DCERPC_NCA_S_FAULT_PIPE_CLOSED),
	_FAULT_STR(DCERPC_NCA_S_FAULT_PIPE_ORDER),
	_FAULT_STR(DCERPC_NCA_S_FAULT_PIPE_DISCIPLINE),
	_FAULT_STR(DCERPC_NCA_S_FAULT_PIPE_COMM_ERROR),
	_FAULT_STR(DCERPC_NCA_S_FAULT_PIPE_MEMORY),
	_FAULT_STR(DCERPC_NCA_S_FAULT_CONTEXT_MISMATCH),
	_FAULT_STR(DCERPC_NCA_S_FAULT_REMOTE_NO_MEMORY),
	_FAULT_STR(DCERPC_NCA_S_INVALID_PRES_CONTEXT_ID),
	_FAULT_STR(DCERPC_NCA_S_UNSUPPORTED_AUTHN_LEVEL),
	_FAULT_STR(DCERPC_NCA_S_UNUSED_1C00001E),
	_FAULT_STR(DCERPC_NCA_S_INVALID_CHECKSUM),
	_FAULT_STR(DCERPC_NCA_S_INVALID_CRC),
	_FAULT_STR(DCERPC_NCA_S_FAULT_USER_DEFINED),
	_FAULT_STR(DCERPC_NCA_S_FAULT_TX_OPEN_FAILED),
	_FAULT_STR(DCERPC_NCA_S_FAULT_CODESET_CONV_ERROR),
	_FAULT_STR(DCERPC_NCA_S_FAULT_OBJECT_NOT_FOUND),
	_FAULT_STR(DCERPC_NCA_S_FAULT_NO_CLIENT_STUB),
	{ NULL, 0 }
#undef _FAULT_STR
};

_PUBLIC_ const char *dcerpc_errstr(TALLOC_CTX *mem_ctx, uint32_t fault_code)
{
	int idx = 0;
	WERROR werr = W_ERROR(fault_code);

	while (dcerpc_faults[idx].errstr != NULL) {
		if (dcerpc_faults[idx].faultcode == fault_code) {
			return dcerpc_faults[idx].errstr;
		}
		idx++;
	}

	return win_errstr(werr);
}

_PUBLIC_ NTSTATUS dcerpc_fault_to_nt_status(uint32_t fault_code)
{
	/* TODO: add more mappings */
	switch (fault_code) {
	case DCERPC_FAULT_OP_RNG_ERROR:
		return NT_STATUS_RPC_PROCNUM_OUT_OF_RANGE;
	case DCERPC_FAULT_UNK_IF:
		return NT_STATUS_RPC_UNKNOWN_IF;
	case DCERPC_FAULT_NDR:
		return NT_STATUS_RPC_BAD_STUB_DATA;
	case DCERPC_FAULT_INVALID_TAG:
		return NT_STATUS_RPC_ENUM_VALUE_OUT_OF_RANGE;
	case DCERPC_FAULT_CONTEXT_MISMATCH:
		return NT_STATUS_RPC_SS_CONTEXT_MISMATCH;
	case DCERPC_FAULT_OTHER:
		return NT_STATUS_RPC_CALL_FAILED;
	case DCERPC_FAULT_ACCESS_DENIED:
		return NT_STATUS_ACCESS_DENIED;
	case DCERPC_FAULT_SEC_PKG_ERROR:
		return NT_STATUS_RPC_SEC_PKG_ERROR;
	}

	return NT_STATUS_RPC_PROTOCOL_ERROR;
}

