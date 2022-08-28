/*
   Unix SMB/CIFS implementation.
   test suite for witness ndr operations

   Copyright (C) Guenther Deschner 2015

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
#include "torture/ndr/ndr.h"
#include "librpc/gen_ndr/ndr_witness.h"
#include "torture/ndr/proto.h"
#include "param/param.h"

static const uint8_t witness_GetInterfaceList_data[] = {
	0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x4f, 0x00, 0x44, 0x00, 0x45, 0x00,
	0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x03, 0x2c, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x05, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x4f, 0x00, 0x44, 0x00, 0x45, 0x00,
	0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x03, 0x2d, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static bool witness_GetInterfaceList_check(struct torture_context *tctx,
					   struct witness_GetInterfaceList *r)
{
	struct witness_interfaceList *l;

	torture_assert(tctx, r->out.interface_list, "r->out.interface_list");

	l = *(r->out.interface_list);

	torture_assert_int_equal(tctx, l->num_interfaces, 2, "l->num_interfaces");
	torture_assert(tctx, l->interfaces, "l->interfaces");

	torture_assert_str_equal(tctx, l->interfaces[0].group_name, "NODE2", "l->interfaces[0].group_name");
	torture_assert_int_equal(tctx, l->interfaces[0].version, -1, "l->interfaces[0].version");
	torture_assert_int_equal(tctx, l->interfaces[0].state, 1, "l->interfaces[0].state");
	torture_assert_str_equal(tctx, l->interfaces[0].ipv4, "192.168.3.44", "l->interfaces[0].state");
	torture_assert_int_equal(tctx, l->interfaces[0].flags, 5, "l->interfaces[0].flags");

	torture_assert_str_equal(tctx, l->interfaces[1].group_name, "NODE1", "l->interfaces[0].group_name");
	torture_assert_int_equal(tctx, l->interfaces[1].version, -1, "l->interfaces[0].version");
	torture_assert_int_equal(tctx, l->interfaces[1].state, 1, "l->interfaces[0].state");
	torture_assert_str_equal(tctx, l->interfaces[1].ipv4, "192.168.3.45", "l->interfaces[0].state");
	torture_assert_int_equal(tctx, l->interfaces[1].flags, 1, "l->interfaces[0].flags");
	torture_assert_werr_ok(tctx, r->out.result, "r->out.result");

	return true;
}

static const uint8_t witness_Register_data_IN[] = {
	0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x73, 0x00, 0x6f, 0x00,
	0x66, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00,
	0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
	0x31, 0x00, 0x39, 0x00, 0x32, 0x00, 0x2e, 0x00, 0x31, 0x00, 0x36, 0x00,
	0x38, 0x00, 0x2e, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x34, 0x00, 0x35, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00, 0x09, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x4d, 0x00, 0x54, 0x00,
	0x48, 0x00, 0x45, 0x00, 0x4c, 0x00, 0x45, 0x00, 0x4e, 0x00, 0x41, 0x00,
	0x00, 0x00
};

static bool witness_Register_check_IN(struct torture_context *tctx,
				      struct witness_Register *r)
{
	torture_assert_int_equal(tctx, r->in.version, 65537, "r->in.version");
	torture_assert_str_equal(tctx, r->in.net_name, "sofs", "r->in.net_name");
	torture_assert_str_equal(tctx, r->in.ip_address, "192.168.3.45", "r->in.ip_address");
	torture_assert_str_equal(tctx, r->in.client_computer_name, "MTHELENA", "r->in.client_computer_name");

	return true;
}

static const uint8_t witness_Register_data_OUT[] = {
	0x00, 0x00, 0x00, 0x00, 0x33, 0x86, 0xb8, 0x3a, 0x57, 0x1e, 0x1a, 0x4c,
	0x85, 0x6c, 0xd1, 0xbc, 0x4b, 0x15, 0xbb, 0xb1, 0x00, 0x00, 0x00, 0x00
};

static bool witness_Register_check_OUT(struct torture_context *tctx,
				       struct witness_Register *r)
{
	struct GUID guid;

	torture_assert(tctx, r->out.context_handle, "r->out.context_handle");
	torture_assert_int_equal(tctx, r->out.context_handle->handle_type, 0, "r->out.context_handle->handle_type");
	torture_assert_ntstatus_ok(tctx, GUID_from_string("3ab88633-1e57-4c1a-856c-d1bc4b15bbb1", &guid), "");
	torture_assert_mem_equal(tctx, &r->out.context_handle->uuid, &guid, sizeof(guid), "r->out.context_handle->uuid");
	torture_assert_werr_ok(tctx, r->out.result, "r->out.result");

	return true;
}

static const uint8_t witness_UnRegister_data_IN[] = {
	0x00, 0x00, 0x00, 0x00, 0x33, 0x86, 0xb8, 0x3a, 0x57, 0x1e, 0x1a, 0x4c,
	0x85, 0x6c, 0xd1, 0xbc, 0x4b, 0x15, 0xbb, 0xb1
};

static bool witness_UnRegister_check_IN(struct torture_context *tctx,
					struct witness_UnRegister *r)
{
	struct GUID guid;

	torture_assert_int_equal(tctx, r->in.context_handle.handle_type, 0, "r->in.context_handle.handle_type");
	torture_assert_ntstatus_ok(tctx, GUID_from_string("3ab88633-1e57-4c1a-856c-d1bc4b15bbb1", &guid), "");
	torture_assert_mem_equal(tctx, &r->in.context_handle.uuid, &guid, sizeof(guid), "r->in.context_handle.uuid");

	return true;
}

static const uint8_t witness_UnRegister_data_OUT[] = {
	0x00, 0x00, 0x00, 0x00
};

static bool witness_UnRegister_check_OUT(struct torture_context *tctx,
					 struct witness_UnRegister *r)
{
	torture_assert_werr_ok(tctx, r->out.result, "r->out.result");

	return true;
}

static const uint8_t witness_AsyncNotify_data_IN[] = {
	0x00, 0x00, 0x00, 0x00, 0xee, 0xf2, 0xb9, 0x1f, 0x4d, 0x2a, 0xf8, 0x4b,
	0xaf, 0x8b, 0xcb, 0x9d, 0x45, 0x29, 0xa9, 0xab
};

static bool witness_AsyncNotify_check_IN(struct torture_context *tctx,
					 struct witness_AsyncNotify *r)
{
	struct GUID guid;

	torture_assert_int_equal(tctx, r->in.context_handle.handle_type, 0, "r->in.context_handle.handle_type");
	torture_assert_ntstatus_ok(tctx, GUID_from_string("1fb9f2ee-2a4d-4bf8-af8b-cb9d4529a9ab", &guid), "");
	torture_assert_mem_equal(tctx, &r->in.context_handle.uuid, &guid, sizeof(guid), "r->in.context_handle.uuid");

	return true;
}

static const uint8_t witness_AsyncNotify_data_OUT[] = {
	0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x12, 0x00, 0x00, 0x00,
	0x12, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x53, 0x00, 0x4f, 0x00,
	0x46, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static bool witness_AsyncNotify_check_OUT(struct torture_context *tctx,
					  struct witness_AsyncNotify *r)
{
	struct witness_notifyResponse *n;
	struct witness_ResourceChange *c;

	torture_assert(tctx, r->out.response, "r->out.response");

	n = *(r->out.response);

	torture_assert_int_equal(tctx, n->type, WITNESS_NOTIFY_RESOURCE_CHANGE, "type");
	torture_assert_int_equal(tctx, n->length, 18, "length");
	torture_assert_int_equal(tctx, n->num, 1, "num");

	c = &n->messages[0].resource_change;

	torture_assert_int_equal(tctx, c->length, 18, "c->length");
	torture_assert_int_equal(tctx, c->type, WITNESS_RESOURCE_STATE_UNAVAILABLE, "c->type");
	torture_assert_str_equal(tctx, c->name, "SOFS", "c->name");

	return true;
}

static const uint8_t witness_AsyncNotify_data_move_OUT[] = {
	0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x24, 0x00, 0x00, 0x00,
	0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x03, 0x2d, 0x00, 0x00, 0x00, 0x00,
	0x38, 0xe8, 0xeb, 0x26, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x9e, 0x60, 0x26,
	0x00, 0x00, 0x00, 0x00
};

static bool witness_AsyncNotify_check_move_OUT(struct torture_context *tctx,
					       struct witness_AsyncNotify *r)
{
	struct witness_notifyResponse *n;
	struct witness_IPaddrInfoList *i;

	torture_assert(tctx, r->out.response, "r->out.response");

	n = *(r->out.response);

	torture_assert_int_equal(tctx, n->type, WITNESS_NOTIFY_CLIENT_MOVE, "type");
	torture_assert_int_equal(tctx, n->length, 36, "length");
	torture_assert_int_equal(tctx, n->num, 1, "num");

	i = &n->messages[0].client_move;

	torture_assert_int_equal(tctx, i->length, 36, "i->length");
	torture_assert_int_equal(tctx, i->reserved, 0, "i->reserved");
	torture_assert_int_equal(tctx, i->num, 1, "i->num");

	torture_assert_int_equal(tctx, i->addr[0].flags, WITNESS_IPADDR_V4, "i->addr[0].flags");
	torture_assert_str_equal(tctx, i->addr[0].ipv4, "192.168.3.45", "i->addr[0].ipv4");
	torture_assert_str_equal(tctx, i->addr[0].ipv6, "0000:0000:38e8:eb26:8e00:0000:009e:6026", "i->addr[0].ipv6");

	return true;
}

static const uint8_t witness_AsyncNotify_data_fuzz1_OUT[] = {
	0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x0C, 0x00, 0x00, 0x00,
	0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static bool witness_AsyncNotify_check_fuzz1_OUT(struct torture_context *tctx,
					        struct witness_AsyncNotify *r)
{
	struct witness_notifyResponse *n;
	struct witness_IPaddrInfoList *i;

	torture_assert(tctx, r->out.response, "r->out.response");

	n = *(r->out.response);

	torture_assert_int_equal(tctx, n->type, WITNESS_NOTIFY_CLIENT_MOVE, "type");
	torture_assert_int_equal(tctx, n->length, 12, "length");
	torture_assert_int_equal(tctx, n->num, 1, "num");

	i = &n->messages[0].client_move;

	torture_assert_int_equal(tctx, i->length, 12, "i->length");
	torture_assert_int_equal(tctx, i->reserved, 0, "i->reserved");
	torture_assert_int_equal(tctx, i->num, 0, "i->num");

	return true;
}

struct torture_suite *ndr_witness_suite(TALLOC_CTX *ctx)
{
	struct torture_suite *suite = torture_suite_create(ctx, "witness");

	torture_suite_add_ndr_pull_fn_test(suite,
					   witness_GetInterfaceList,
					   witness_GetInterfaceList_data,
					   NDR_OUT,
					   witness_GetInterfaceList_check);

	torture_suite_add_ndr_pull_fn_test(suite,
					   witness_Register,
					   witness_Register_data_IN,
					   NDR_IN,
					   witness_Register_check_IN);

	torture_suite_add_ndr_pull_fn_test(suite,
					   witness_Register,
					   witness_Register_data_OUT,
					   NDR_OUT,
					   witness_Register_check_OUT);

	torture_suite_add_ndr_pull_fn_test(suite,
					   witness_UnRegister,
					   witness_UnRegister_data_IN,
					   NDR_IN,
					   witness_UnRegister_check_IN);

	torture_suite_add_ndr_pull_fn_test(suite,
					   witness_UnRegister,
					   witness_UnRegister_data_OUT,
					   NDR_OUT,
					   witness_UnRegister_check_OUT);

	torture_suite_add_ndr_pull_fn_test(suite,
					   witness_AsyncNotify,
					   witness_AsyncNotify_data_IN,
					   NDR_IN,
					   witness_AsyncNotify_check_IN);

	torture_suite_add_ndr_pull_fn_test(suite,
					   witness_AsyncNotify,
					   witness_AsyncNotify_data_OUT,
					   NDR_OUT,
					   witness_AsyncNotify_check_OUT);

	torture_suite_add_ndr_pullpush_fn_test_flags(suite,
					    witness_AsyncNotify,
					    witness_AsyncNotify_data_OUT,
					    NDR_OUT,
					    0,
					    witness_AsyncNotify_check_OUT);

	torture_suite_add_ndr_pullpush_fn_test_flags(suite,
					    witness_AsyncNotify,
					    witness_AsyncNotify_data_move_OUT,
					    NDR_OUT,
					    0,
					    witness_AsyncNotify_check_move_OUT);

	torture_suite_add_ndr_pull_fn_test(suite,
					   witness_AsyncNotify,
					   witness_AsyncNotify_data_fuzz1_OUT,
					   NDR_OUT,
					   witness_AsyncNotify_check_fuzz1_OUT);

	torture_suite_add_ndr_pullpush_fn_test_flags(suite,
					    witness_AsyncNotify,
					    witness_AsyncNotify_data_fuzz1_OUT,
					    NDR_OUT,
					    0,
					    witness_AsyncNotify_check_fuzz1_OUT);

	return suite;
}
