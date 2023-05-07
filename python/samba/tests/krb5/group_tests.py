#!/usr/bin/env python3
# Unix SMB/CIFS implementation.
# Copyright (C) Stefan Metzmacher 2020
# Copyright (C) Catalyst.Net Ltd 2022
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import sys
import os

sys.path.insert(0, 'bin/python')
os.environ['PYTHONUNBUFFERED'] = '1'

import re

from enum import Enum
from functools import partial

import ldb

from samba.dcerpc import krb5pac, netlogon, samr, security
from samba.dsdb import (
    GTYPE_SECURITY_DOMAIN_LOCAL_GROUP,
    GTYPE_SECURITY_GLOBAL_GROUP,
    GTYPE_SECURITY_UNIVERSAL_GROUP,
)
from samba.tests import DynamicTestCase, env_get_var_value
from samba.tests.krb5 import kcrypto
from samba.tests.krb5.kdc_base_test import KDCBaseTest
from samba.tests.krb5.raw_testcase import RawKerberosTest
from samba.tests.krb5.rfc4120_constants import (
    KRB_TGS_REP,
    NT_PRINCIPAL,
)

SidType = RawKerberosTest.SidType

global_asn1_print = False
global_hexdump = False


class GroupType(Enum):
    GLOBAL = GTYPE_SECURITY_GLOBAL_GROUP
    DOMAIN_LOCAL = GTYPE_SECURITY_DOMAIN_LOCAL_GROUP
    UNIVERSAL = GTYPE_SECURITY_UNIVERSAL_GROUP


# This simple class encapsulates the DN and SID of a Principal.
class Principal:
    def __init__(self, dn, sid):
        if not isinstance(dn, ldb.Dn):
            raise AssertionError(f'expected {dn} to be an ldb.Dn')

        self.dn = dn
        self.sid = sid


@DynamicTestCase
class GroupTests(KDCBaseTest):
    # A placeholder object that represents the user account undergoing testing.
    user = object()

    # Constants for group SID attributes.
    default_attrs = (security.SE_GROUP_MANDATORY |
                     security.SE_GROUP_ENABLED_BY_DEFAULT |
                     security.SE_GROUP_ENABLED)
    resource_attrs = default_attrs | security.SE_GROUP_RESOURCE

    asserted_identity = security.SID_AUTHENTICATION_AUTHORITY_ASSERTED_IDENTITY

    def setUp(self):
        super().setUp()
        self.do_asn1_print = global_asn1_print
        self.do_hexdump = global_hexdump

    @classmethod
    def setUpDynamicTestCases(cls):
        FILTER = env_get_var_value('FILTER', allow_missing=True)
        SKIP_INVALID = env_get_var_value('SKIP_INVALID', allow_missing=True)

        for case in cls.cases:
            invalid = case.pop('configuration_invalid', False)
            if SKIP_INVALID and invalid:
                # Some group setups are invalid on Windows, so we allow them to
                # be skipped.
                continue
            name = case.pop('test')
            if FILTER and not re.search(FILTER, name):
                continue
            name = re.sub(r'\W+', '_', name)

            cls.generate_dynamic_test('test_group', name,
                                      dict(case))

    # Enable or disable resource SID compression on the krbtgt
    # account. Depending on how the KDC chooses to handle SID compression, this
    # may or may not have any real effect.
    def set_krbtgt_sid_compression(self, compression):
        krbtgt_creds = self.get_krbtgt_creds()
        krbtgt_dn = krbtgt_creds.get_dn()

        samdb = self.get_samdb()

        # Get the current supported encryption types of the krbtgt account.
        res = samdb.search(krbtgt_dn,
                           scope=ldb.SCOPE_BASE,
                           attrs=['msDS-SupportedEncryptionTypes'])
        orig_msg = res[0]
        krbtgt_enctypes = orig_msg.get(
            'msDS-SupportedEncryptionTypes', idx=0)
        if krbtgt_enctypes is None:
            # Setting the enctypes isn't likely to accomplish anything.
            return

        krbtgt_enctypes = int(krbtgt_enctypes)

        # Enable or disable the compression bit.
        if compression:
            set_krbtgt_enctypes = krbtgt_enctypes | (
                security.KERB_ENCTYPE_RESOURCE_SID_COMPRESSION_DISABLED)
        else:
            set_krbtgt_enctypes = krbtgt_enctypes & ~(
                security.KERB_ENCTYPE_RESOURCE_SID_COMPRESSION_DISABLED)

        if krbtgt_enctypes == set_krbtgt_enctypes:
            # Nothing to do.
            return

        msg = ldb.Message(krbtgt_dn)
        msg['msDS-SupportedEncryptionTypes'] = ldb.MessageElement(
            str(set_krbtgt_enctypes),
            ldb.FLAG_MOD_REPLACE,
            'msDS-SupportedEncryptionTypes')

        # Clean up the change afterwards.
        diff = samdb.msg_diff(msg, orig_msg)
        self.addCleanup(samdb.modify, diff)

        samdb.modify(msg)

        # Make sure the value remains as we set it.
        res = samdb.search(krbtgt_dn,
                           scope=ldb.SCOPE_BASE,
                           attrs=['msDS-SupportedEncryptionTypes'])
        new_krbtgt_enctypes = res[0].get(
            'msDS-SupportedEncryptionTypes', idx=0)
        self.assertIsNotNone(new_krbtgt_enctypes)
        new_krbtgt_enctypes = int(new_krbtgt_enctypes)
        self.assertEqual(set_krbtgt_enctypes, new_krbtgt_enctypes,
                         'failed to set krbtgt supported enctypes')

    # Get a ticket with the SIDs in the PAC replaced with ones we specify. This
    # is useful for creating arbitrary tickets that can be used to perform a
    # TGS-REQ.
    def ticket_with_sids(self, ticket, new_sids, domain_sid):
        krbtgt_creds = self.get_krbtgt_creds()
        krbtgt_key = self.TicketDecryptionKey_from_creds(krbtgt_creds)

        checksum_keys = {
            krb5pac.PAC_TYPE_KDC_CHECKSUM: krbtgt_key
        }

        modify_pac_fn = partial(self.set_pac_sids,
                                new_sids=new_sids,
                                domain_sid=domain_sid)

        return self.modified_ticket(ticket,
                                    modify_pac_fn=modify_pac_fn,
                                    checksum_keys=checksum_keys)

    # Replace the SIDs in a PAC with 'new_sids'.
    def set_pac_sids(self, pac, new_sids, domain_sid):
        base_sids = []
        extra_sids = []
        resource_sids = []

        resource_domain = None

        # Filter our SIDs into three arrays depending on their ultimate
        # location in the PAC.
        for sid, sid_type, attrs in new_sids:
            if sid_type is self.SidType.BASE_SID:
                domain, rid = sid.rsplit('-', 1)
                self.assertEqual(domain_sid, domain,
                                 'base SIDs must be in our domain')

                base_sid = samr.RidWithAttribute()
                base_sid.rid = int(rid)
                base_sid.attributes = attrs

                base_sids.append(base_sid)
            elif sid_type is self.SidType.EXTRA_SID:
                extra_sid = netlogon.netr_SidAttr()
                extra_sid.sid = security.dom_sid(sid)
                extra_sid.attributes = attrs

                extra_sids.append(extra_sid)
            elif sid_type is self.SidType.RESOURCE_SID:
                domain, rid = sid.rsplit('-', 1)
                if resource_domain is None:
                    resource_domain = domain
                else:
                    self.assertEqual(resource_domain, domain,
                                     'resource SIDs must share the same '
                                     'domain')

                resource_sid = samr.RidWithAttribute()
                resource_sid.rid = int(rid)
                resource_sid.attributes = attrs

                resource_sids.append(resource_sid)
            else:
                self.fail(f'invalid SID type {sid_type}')

        pac_buffers = pac.buffers
        for pac_buffer in pac_buffers:
            # Find the LOGON_INFO PAC buffer.
            if pac_buffer.type == krb5pac.PAC_TYPE_LOGON_INFO:
                logon_info = pac_buffer.info.info

                # Add Extra SIDs and set the EXTRA_SIDS flag as needed.
                logon_info.info3.sidcount = len(extra_sids)
                if extra_sids:
                    logon_info.info3.sids = extra_sids
                    logon_info.info3.base.user_flags |= (
                        netlogon.NETLOGON_EXTRA_SIDS)
                else:
                    logon_info.info3.sids = None
                    logon_info.info3.base.user_flags &= ~(
                        netlogon.NETLOGON_EXTRA_SIDS)

                # Add Base SIDs.
                logon_info.info3.base.groups.count = len(base_sids)
                if base_sids:
                    logon_info.info3.base.groups.rids = base_sids
                else:
                    logon_info.info3.base.groups.rids = None

                # Add Resource SIDs and set the RESOURCE_GROUPS flag as needed.
                logon_info.resource_groups.groups.count = len(resource_sids)
                if resource_sids:
                    resource_domain = security.dom_sid(resource_domain)
                    logon_info.resource_groups.domain_sid = resource_domain
                    logon_info.resource_groups.groups.rids = resource_sids
                    logon_info.info3.base.user_flags |= (
                        netlogon.NETLOGON_RESOURCE_GROUPS)
                else:
                    logon_info.resource_groups.domain_sid = None
                    logon_info.resource_groups.groups.rids = None
                    logon_info.info3.base.user_flags &= ~(
                        netlogon.NETLOGON_RESOURCE_GROUPS)

                break
        else:
            self.fail('no LOGON_INFO PAC buffer')

        pac.buffers = pac_buffers

        return pac

    # A list of test cases.
    cases = [
        # AS-REQ tests.
        {
            'test': 'universal; as-req to krbtgt',
            'groups': {
                # A Universal group containing the user.
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            # Make an AS-REQ to the krbtgt with the user's account.
            'as:to_krbtgt': True,
            'as:expected': {
                # Ignoring the user ID, or base RID, expect the PAC to contain
                # precisely the following SIDS in any order:
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'universal; as-req to service',
            'groups': {
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            # The same again, but this time perform the AS-REQ to a service.
            'as:to_krbtgt': False,
            'as:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'global; as-req to krbtgt',
            'groups': {
                # The behaviour should be the same with a Global group.
                'foo': (GroupType.GLOBAL, {user}),
            },
            'as:to_krbtgt': True,
            'as:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'global; as-req to service',
            'groups': {
                'foo': (GroupType.GLOBAL, {user}),
            },
            'as:to_krbtgt': False,
            'as:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'domain-local; as-req to krbtgt',
            'groups': {
                # A Domain-local group containing the user.
                'foo': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'as:to_krbtgt': True,
            'as:expected': {
                # A TGT will not contain domain-local groups the user belongs
                # to.
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'domain-local; compression; as-req to service',
            'groups': {
                'foo': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'as:to_krbtgt': False,
            'as:expected': {
                # However, a service ticket will include domain-local
                # groups. The account supports SID compression, so they are
                # added as resource SIDs.
                ('foo', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'domain-local; no compression; as-req to service',
            'groups': {
                'foo': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'as:to_krbtgt': False,
            # This time, the target account disclaims support for SID
            # compression.
            'as:compression': False,
            'as:expected': {
                # The SIDs in the PAC are the same, except the group SID is
                # placed in Extra SIDs, not Resource SIDs.
                ('foo', SidType.EXTRA_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested domain-local; as-req to krbtgt',
            'groups': {
                # A Universal group containing a Domain-local group containing
                # the user.
                'universal': (GroupType.UNIVERSAL, {'dom-local'}),
                'dom-local': (GroupType.DOMAIN_LOCAL, {user}),
            },
            # It is not possible in Windows for a Universal group to contain a
            # Domain-local group without exploiting bugs. This flag provides a
            # convenient means by which these tests can be skipped.
            'configuration_invalid': True,
            'as:to_krbtgt': True,
            'as:expected': {
                # While Windows would exclude the universal group from the PAC,
                # expecting its inclusion is more sensible on the whole.
                ('universal', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested domain-local; compression; as-req to service',
            'groups': {
                'universal': (GroupType.UNIVERSAL, {'dom-local'}),
                'dom-local': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'configuration_invalid': True,
            'as:to_krbtgt': False,
            'as:expected': {
                # A service ticket is expected to include both SIDs.
                ('universal', SidType.BASE_SID, default_attrs),
                ('dom-local', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested domain-local; no compression; as-req to service',
            'groups': {
                'universal': (GroupType.UNIVERSAL, {'dom-local'}),
                'dom-local': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'configuration_invalid': True,
            'as:to_krbtgt': False,
            'as:compression': False,
            'as:expected': {
                # As before, but disclaiming SID compression support, so the
                # domain-local SID goes in Extra SIDs.
                ('universal', SidType.BASE_SID, default_attrs),
                ('dom-local', SidType.EXTRA_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested universal; as-req to krbtgt',
            'groups': {
                # A similar scenario, except flipped around: a Domain-local
                # group containing a Universal group containing the user.
                'dom-local': (GroupType.DOMAIN_LOCAL, {'universal'}),
                'universal': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': True,
            'as:expected': {
                # Expect the Universal group's inclusion in the PAC.
                ('universal', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested universal; compression; as-req to service',
            'groups': {
                'dom-local': (GroupType.DOMAIN_LOCAL, {'universal'}),
                'universal': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': False,
            'as:expected': {
                # Expect a service ticket to contain both SIDs.
                ('universal', SidType.BASE_SID, default_attrs),
                ('dom-local', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested universal; no compression; as-req to service',
            'groups': {
                'dom-local': (GroupType.DOMAIN_LOCAL, {'universal'}),
                'universal': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': False,
            'as:compression': False,
            'as:expected': {
                # As before, but disclaiming SID compression support, so the
                # domain-local SID goes in Extra SIDs.
                ('universal', SidType.BASE_SID, default_attrs),
                ('dom-local', SidType.EXTRA_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        # TGS-REQ tests.
        {
            'test': 'tgs-req to krbtgt',
            'groups': {
                # A Universal group containing the user.
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': True,
            'as:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            # Make a TGS-REQ to the krbtgt with the user's account.
            'tgs:to_krbtgt': True,
            'tgs:expected': {
                # Expect the same results as with an AS-REQ.
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'tgs-req to service',
            'groups': {
                # A Universal group containing the user.
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': True,
            'as:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            # Make a TGS-REQ to a service with the user's account.
            'tgs:to_krbtgt': False,
            'tgs:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'domain-local; tgs-req to krbtgt',
            'groups': {
                # A Domain-local group containing the user.
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': True,
            'as:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:to_krbtgt': True,
            'tgs:expected': {
                # Expect the same results as with an AS-REQ.
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'domain-local; compression; tgs-req to service',
            'groups': {
                # A Domain-local group containing the user.
                'foo': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'as:to_krbtgt': True,
            'as:expected': {
                # The Domain-local group is not present in the PAC after an
                # AS-REQ.
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:to_krbtgt': False,
            'tgs:expected': {
                # Now it's added as a resource SID after the TGS-REQ.
                ('foo', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'domain-local; no compression; tgs-req to service',
            'groups': {
                # A Domain-local group containing the user.
                'foo': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'as:to_krbtgt': True,
            # This time, the target account disclaims support for SID
            # compression.
            'as:expected': {
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:to_krbtgt': False,
            'tgs:compression': False,
            'tgs:expected': {
                # The SIDs in the PAC are the same, except the group SID is
                # placed in Extra SIDs, not Resource SIDs.
                ('foo', SidType.EXTRA_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'exclude asserted identity; tgs-req to krbtgt',
            'groups': {
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': True,
            'tgs:sids': {
                # Remove the Asserted Identity SID from the PAC.
                ('foo', SidType.BASE_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                # It should not be re-added in the TGS-REQ.
                ('foo', SidType.BASE_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'exclude asserted identity; tgs-req to service',
            'groups': {
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': True,
            # Nor should it be re-added if the TGS-REQ is directed to a
            # service.
            'tgs:to_krbtgt': False,
            'tgs:sids': {
                ('foo', SidType.BASE_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'exclude claims valid; tgs-req to krbtgt',
            'groups': {
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': True,
            'tgs:sids': {
                # Remove the Claims Valid SID from the PAC.
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
            },
            'tgs:expected': {
                # It should not be re-added in the TGS-REQ.
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
            },
        },
        {
            'test': 'exclude claims valid; tgs-req to service',
            'groups': {
                'foo': (GroupType.UNIVERSAL, {user}),
            },
            'as:to_krbtgt': True,
            # Nor should it be re-added if the TGS-REQ is directed to a
            # service.
            'tgs:to_krbtgt': False,
            'tgs:sids': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
            },
            'tgs:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
            },
        },
        {
            'test': 'user group removal; tgs-req to krbtgt',
            'groups': {
                # The user has been removed from the group...
                'foo': (GroupType.UNIVERSAL, {}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': True,
            'tgs:sids': {
                # ...but the user's PAC still contains the group SID.
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                # The group SID should not be removed when a TGS-REQ is
                # performed.
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'user group removal; tgs-req to service',
            'groups': {
                'foo': (GroupType.UNIVERSAL, {}),
            },
            'as:to_krbtgt': True,
            # Likewise, but to a service.
            'tgs:to_krbtgt': False,
            'tgs:sids': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested group removal; tgs-req to krbtgt',
            'groups': {
                # A Domain-local group contains a Universal group, of which the
                # user is no longer a member...
                'dom-local': (GroupType.DOMAIN_LOCAL, {'universal'}),
                'universal': (GroupType.UNIVERSAL, {}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': True,
            'tgs:sids': {
                # ...but the user's PAC still contains the group SID.
                ('universal', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                # The group SID should not be removed when a TGS-REQ is
                # performed.
                ('universal', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested group removal; compression; tgs-req to service',
            'groups': {
                # A Domain-local group contains a Universal group, of which the
                # user is no longer a member...
                'dom-local': (GroupType.DOMAIN_LOCAL, {'universal'}),
                'universal': (GroupType.UNIVERSAL, {}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': False,
            'tgs:sids': {
                # ...but the user's PAC still contains the group SID.
                ('universal', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                # Both SIDs should be present in the PAC when a TGS-REQ is
                # performed.
                ('universal', SidType.BASE_SID, default_attrs),
                ('dom-local', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'nested group removal; no compression; tgs-req to service',
            'groups': {
                'dom-local': (GroupType.DOMAIN_LOCAL, {'universal'}),
                'universal': (GroupType.UNIVERSAL, {}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': False,
            # The same again, but with the server not supporting compression.
            'tgs:compression': False,
            'tgs:sids': {
                ('universal', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                # The domain-local SID will go into Extra SIDs.
                ('universal', SidType.BASE_SID, default_attrs),
                ('dom-local', SidType.EXTRA_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'resource sids given; compression; tgs-req to krbtgt',
            'groups': {
                # A couple of independent domain-local groups.
                'dom-local-0': (GroupType.DOMAIN_LOCAL, {}),
                'dom-local-1': (GroupType.DOMAIN_LOCAL, {}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': True,
            'tgs:compression': True,
            'tgs:sids': {
                # The TGT contains two resource SIDs for the domain-local
                # groups.
                ('dom-local-0', SidType.RESOURCE_SID, resource_attrs),
                ('dom-local-1', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                # The resource SIDs remain after performing a TGS-REQ to the
                # krbtgt.
                ('dom-local-0', SidType.RESOURCE_SID, resource_attrs),
                ('dom-local-1', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'resource sids given; no compression; tgs-req to krbtgt',
            'groups': {
                'dom-local-0': (GroupType.DOMAIN_LOCAL, {}),
                'dom-local-1': (GroupType.DOMAIN_LOCAL, {}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': True,
            # Compression is disabled on the krbtgt account...
            'tgs:compression': False,
            'tgs:sids': {
                ('dom-local-0', SidType.RESOURCE_SID, resource_attrs),
                ('dom-local-1', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                # ...and the resource SIDs remain.
                ('dom-local-0', SidType.RESOURCE_SID, resource_attrs),
                ('dom-local-1', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        {
            'test': 'resource sids given; compression; tgs-req to service',
            'groups': {
                'dom-local-0': (GroupType.DOMAIN_LOCAL, {}),
                'dom-local-1': (GroupType.DOMAIN_LOCAL, {}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': False,
            'tgs:sids': {
                ('dom-local-0', SidType.RESOURCE_SID, resource_attrs),
                ('dom-local-1', SidType.RESOURCE_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
            'tgs:expected': {
                # The resource SIDs are removed upon issuing a service ticket.
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
                (security.SID_CLAIMS_VALID, SidType.EXTRA_SID, default_attrs),
            },
        },
        # Testing operability with older Samba versions.
        {
            'test': 'domain-local; Samba 4.17; tgs-req to krbtgt',
            'groups': {
                'foo': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'as:to_krbtgt': True,
            'tgs:to_krbtgt': True,
            'tgs:compression': False,
            'tgs:sids': {
                # In Samba 4.17, domain-local groups are contained within the
                # TGT, and do not have the SE_GROUP_RESOURCE bit set.
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
            },
            'tgs:expected': {
                # After the TGS-REQ, the domain-local group remains in the PAC
                # with its original attributes.
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
            },
        },
        {
            'test': 'domain-local; Samba 4.17; tgs-req to service',
            'groups': {
                'foo': (GroupType.DOMAIN_LOCAL, {user}),
            },
            'as:to_krbtgt': True,
            # The same scenario, but requesting a service ticket.
            'tgs:to_krbtgt': False,
            'tgs:compression': False,
            'tgs:sids': {
                ('foo', SidType.BASE_SID, default_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
            },
            'tgs:expected': {
                # The domain-local group remains in the PAC...
                ('foo', SidType.BASE_SID, default_attrs),
                # and another copy is added in Extra SIDs. This one has the
                # SE_GROUP_RESOURCE bit set.
                ('foo', SidType.EXTRA_SID, resource_attrs),
                (asserted_identity, SidType.EXTRA_SID, default_attrs),
                (security.DOMAIN_RID_USERS, SidType.BASE_SID, default_attrs),
            },
        },
    ]

    # Create a new group and return a Principal object representing it.
    def create_group_principal(self, samdb, group_type):
        name = self.get_new_username()
        dn = self.create_group(samdb, name, gtype=group_type.value)
        sid = self.get_objectSid(samdb, dn)

        return Principal(ldb.Dn(samdb, dn), sid)

    claims_valid_sid = (security.SID_CLAIMS_VALID,
                        SidType.EXTRA_SID,
                        default_attrs)

    # Return SIDs from principal placeholders based on a supplied mapping.
    def map_sids(self, sids, mapping, domain_sid):
        if sids is None:
            return None

        mapped_sids = set()
        for sid, sid_type, attrs in sids:
            if isinstance(sid, int):
                # If it's an integer, we assume it's a RID, and prefix the
                # domain SID.
                sid = f'{domain_sid}-{sid}'
            elif sid in mapping:
                # Or if we have a mapping for it, apply that. Otherwise leave
                # it unmodified.
                sid = mapping[sid].sid

            # There's no point expecting the 'Claims Valid' SID to be present
            # if we don't support claims. Filter it out to give the tests a
            # chance of passing.
            if not self.kdc_claims_support and (
                    sid == security.SID_CLAIMS_VALID):
                continue

            mapped_sids.add((sid, sid_type, attrs))

        return mapped_sids

    # Create an arrangement on groups based on a configuration specified in a
    # test case. 'user_principal' is a principal representing the user account.
    def setup_groups(self, samdb, group_setup, user_principal):
        # Initialiase the group mapping with the user principal.
        groups = {self.user: user_principal}

        # Create each group and add it to the group mapping.
        for group_id, (group_type, _) in group_setup.items():
            self.assertIsNot(group_id, self.user,
                             "don't specify user placeholder")
            self.assertNotIn(group_id, groups,
                             'group ID specified more than once')
            groups[group_id] = self.create_group_principal(samdb, group_type)

        # Map a group ID to that group's DN, and generate an
        # understandable error message if the mapping fails.
        def group_id_to_dn(group_id):
            try:
                group = groups[group_id]
            except KeyError:
                self.fail(f"included group member '{group_id}', but it is not "
                          f"specified in {groups.keys()}")
            else:
                return str(group.dn)

        # Populate each group's members.
        for group_id, (_, members) in group_setup.items():
            # Get the group's DN and the mapped DNs of its members.
            dn = groups[group_id].dn
            principal_members = map(group_id_to_dn, members)

            # Add the members to the group.
            self.add_to_group(principal_members, dn, 'member',
                              expect_attr=False)

        # Return the mapping from group IDs to principals.
        return groups

    # Get the credentials and server principal name of either the krbtgt, or a
    # specially created account, with resource SID compression either supported
    # or unsupported.
    def get_target(self, to_krbtgt, compression):
        if to_krbtgt:
            self.set_krbtgt_sid_compression(compression)
            creds = self.get_krbtgt_creds()
            sname = self.get_krbtgt_sname()
        else:
            creds = self.get_cached_creds(
                account_type=self.AccountType.COMPUTER,
                opts={
                    'sid_compression_support': compression,
                })
            target_name = creds.get_username()

            if target_name[-1] == '$':
                target_name = target_name[:-1]
            sname = self.PrincipalName_create(
                name_type=NT_PRINCIPAL,
                names=['host', target_name])

        return creds, sname

    # This is the main function to handle a single testcase.
    def _test_group_with_args(self, case):
        # The group arrangement for the test.
        group_setup = case.pop('groups')

        # Whether the AS-REQ or TGS-REQ should be directed to the krbtgt.
        as_to_krbtgt = case.pop('as:to_krbtgt')
        tgs_to_krbtgt = case.pop('tgs:to_krbtgt', None)

        # Whether the target server of the AS-REQ or TGS-REQ should support
        # resource SID compression.
        as_compression = case.pop('as:compression', None)
        tgs_compression = case.pop('tgs:compression', None)

        # Optional SIDs to replace those in the PAC prior to a TGS-REQ.
        tgs_sids = case.pop('tgs:sids', None)

        # The SIDs we expect to see in the PAC after a AS-REQ or a TGS-REQ.
        as_expected = case.pop('as:expected', None)
        tgs_expected = case.pop('tgs:expected', None)

        # There should be no parameters remaining in the testcase.
        self.assertFalse(case, 'unexpected parameters in testcase')

        if as_expected is None:
            self.assertIsNotNone(tgs_expected,
                                 'no set of expected SIDs is provided')

        if as_to_krbtgt is None:
            as_to_krbtgt = False

        if not as_to_krbtgt:
            self.assertIsNone(tgs_expected,
                              "if we're performing a TGS-REQ, then AS-REQ "
                              "should be directed to the krbtgt")

        if tgs_to_krbtgt is None:
            tgs_to_krbtgt = False
        else:
            self.assertIsNotNone(tgs_expected,
                                 'specified TGS request to krbtgt, but no '
                                 'expected SIDs provided')

        if tgs_compression is not None:
            self.assertIsNotNone(tgs_expected,
                                 'specified compression for TGS request, but '
                                 'no expected SIDs provided')

        samdb = self.get_samdb()

        domain_sid = samdb.get_domain_sid()

        # Create the user account. It needs to be freshly created rather than
        # cached because we will probably add it to one or more groups.
        user_creds = self.get_cached_creds(
            account_type=self.AccountType.USER,
            use_cache=False)
        user_dn = user_creds.get_dn()
        user_sid = self.get_objectSid(samdb, user_dn)
        user_name = user_creds.get_username()
        salt = user_creds.get_salt()

        cname = self.PrincipalName_create(name_type=NT_PRINCIPAL,
                                          names=user_name.split('/'))

        preauth_key = self.PasswordKey_from_creds(user_creds,
                                                  kcrypto.Enctype.AES256)

        ts_enc_padata = self.get_enc_timestamp_pa_data_from_key(preauth_key)
        padata = [ts_enc_padata]

        target_creds, sname = self.get_target(as_to_krbtgt, as_compression)
        decryption_key = self.TicketDecryptionKey_from_creds(target_creds)

        target_supported_etypes = target_creds.tgs_supported_enctypes
        realm = target_creds.get_realm()

        user_principal = Principal(user_dn, user_sid)
        groups = self.setup_groups(samdb, group_setup, user_principal)
        del group_setup

        expected_groups = self.map_sids(as_expected, groups, domain_sid)
        tgs_sids_mapped = self.map_sids(tgs_sids, groups, domain_sid)
        tgs_expected_mapped = self.map_sids(tgs_expected, groups, domain_sid)

        till = self.get_KerberosTime(offset=36000)
        kdc_options = '0'

        etypes = self.get_default_enctypes()

        # Perform an AS-REQ with the user account.
        as_rep, kdc_exchange_dict = self._test_as_exchange(
            cname=cname,
            realm=realm,
            sname=sname,
            till=till,
            client_as_etypes=etypes,
            expected_error_mode=0,
            expected_crealm=realm,
            expected_cname=cname,
            expected_srealm=realm,
            expected_sname=sname,
            expected_salt=salt,
            etypes=etypes,
            padata=padata,
            kdc_options=kdc_options,
            expected_account_name=user_name,
            expected_groups=expected_groups,
            expected_sid=user_sid,
            expected_domain_sid=domain_sid,
            expected_supported_etypes=target_supported_etypes,
            preauth_key=preauth_key,
            ticket_decryption_key=decryption_key)
        self.check_as_reply(as_rep)

        ticket = kdc_exchange_dict['rep_ticket_creds']

        if tgs_expected is None:
            # We're not performing a TGS-REQ, so we're done.
            self.assertIsNone(tgs_sids,
                              'provided SIDs to populate PAC for TGS-REQ, but '
                              'failed to specify expected SIDs')
            return

        if tgs_sids is not None:
            # Replace the SIDs in the PAC with the ones provided by the test.
            ticket = self.ticket_with_sids(ticket, tgs_sids_mapped, domain_sid)

        target_creds, sname = self.get_target(tgs_to_krbtgt, tgs_compression)
        decryption_key = self.TicketDecryptionKey_from_creds(target_creds)

        subkey = self.RandomKey(ticket.session_key.etype)

        # Perform a TGS-REQ with the user account.

        kdc_exchange_dict = self.tgs_exchange_dict(
            expected_crealm=ticket.crealm,
            expected_cname=cname,
            expected_srealm=realm,
            expected_sname=sname,
            expected_account_name=user_name,
            expected_groups=tgs_expected_mapped,
            expected_sid=user_sid,
            expected_domain_sid=domain_sid,
            expected_supported_etypes=target_supported_etypes,
            ticket_decryption_key=decryption_key,
            check_rep_fn=self.generic_check_kdc_rep,
            check_kdc_private_fn=self.generic_check_kdc_private,
            tgt=ticket,
            authenticator_subkey=subkey,
            kdc_options=kdc_options)

        rep = self._generic_kdc_exchange(kdc_exchange_dict,
                                         cname=None,
                                         realm=realm,
                                         sname=sname,
                                         till_time=till,
                                         etypes=etypes)
        self.check_reply(rep, KRB_TGS_REP)


if __name__ == '__main__':
    global_asn1_print = False
    global_hexdump = False
    import unittest
    unittest.main()
