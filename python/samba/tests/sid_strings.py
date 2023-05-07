# Unix SMB/CIFS implementation.
# Copyright (C) Catalyst.NET Ltd 2022
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

import os
import random
import string
import sys
import time

import ldb

from samba import param

from samba.auth import system_session
from samba.credentials import Credentials
from samba.dcerpc import security
from samba.ndr import ndr_unpack
from samba.samdb import SamDB
from samba.tests import (
    DynamicTestCase,
    TestCase,
    delete_force,
    env_get_var_value,
)

sys.path.insert(0, 'bin/python')
os.environ['PYTHONUNBUFFERED'] = '1'


@DynamicTestCase
class SidStringTests(TestCase):
    @classmethod
    def setUpDynamicTestCases(cls):
        if env_get_var_value('CHECK_ALL_COMBINATIONS',
                             allow_missing=True):
            for x in string.ascii_uppercase:
                for y in string.ascii_uppercase:
                    code = x + y
                    if code not in cls.cases:
                        cls.cases[code] = None

        for code, expected_sid in cls.cases.items():
            name = code

            cls.generate_dynamic_test('test_sid_string', name,
                                      code, expected_sid)

    @classmethod
    def setUpClass(cls):
        super().setUpClass()

        server = os.environ['DC_SERVER']
        host = f'ldap://{server}'

        lp = param.LoadParm()
        lp.load(os.environ['SMB_CONF_PATH'])

        creds = Credentials()
        creds.guess(lp)
        creds.set_username(env_get_var_value('DC_USERNAME'))
        creds.set_password(env_get_var_value('DC_PASSWORD'))

        cls.ldb = SamDB(host, credentials=creds,
                        session_info=system_session(lp), lp=lp)
        cls.base_dn = cls.ldb.domain_dn()
        cls.schema_dn = cls.ldb.get_schema_basedn().get_linearized()

    def _test_sid_string_with_args(self, code, expected_sid):
        random_suffix = random.randint(0, 100000)
        timestamp = time.strftime('%s', time.gmtime())

        class_name = f'my-Sid-String-Class{timestamp}{random_suffix}'
        class_ldap_display_name = class_name.replace('-', '')

        class_dn = f'CN={class_name},{self.schema_dn}'

        governs_id = f'1.3.6.1.4.1.7165.4.6.2.9.{random_suffix}'
        if expected_sid is not None:
            # Append the RID to our OID to ensure more uniqueness.
            rid = expected_sid.rsplit('-', 1)[1]
            governs_id += f'.{rid}'

        ldif = f'''
dn: {class_dn}
objectClass: classSchema
cn: {class_name}
governsId: {governs_id}
subClassOf: top
possSuperiors: domainDNS
defaultSecurityDescriptor: O:{code}
'''
        try:
            self.ldb.add_ldif(ldif)
        except ldb.LdbError as err:
            num, _ = err.args
            self.assertEqual(num, ldb.ERR_UNWILLING_TO_PERFORM)
            self.assertIsNone(expected_sid)
            return

        # Search for created objectclass
        res = self.ldb.search(class_dn, scope=ldb.SCOPE_BASE,
                              attrs=['defaultSecurityDescriptor'])
        self.assertEqual(1, len(res))
        self.assertEqual(res[0].get('defaultSecurityDescriptor', idx=0),
                         f'O:{code}'.encode('utf-8'))

        ldif = '''
dn:
changetype: modify
add: schemaUpdateNow
schemaUpdateNow: 1
'''
        self.ldb.modify_ldif(ldif)

        object_name = f'sddl_{timestamp}_{random_suffix}'
        object_dn = f'CN={object_name},{self.base_dn}'

        ldif = f'''
dn: {object_dn}
objectClass: {class_ldap_display_name}
cn: {object_name}
'''
        self.ldb.add_ldif(ldif)

        # Search for created object
        res = self.ldb.search(object_dn, scope=ldb.SCOPE_BASE,
                              attrs=['nTSecurityDescriptor'])
        self.assertEqual(1, len(res))

        # Delete the object
        delete_force(self.ldb, object_dn)

        data = res[0].get('nTSecurityDescriptor', idx=0)
        descriptor = ndr_unpack(security.descriptor, data)

        domain_sid = self.ldb.get_domain_sid()

        if expected_sid is None:
            expected_sid = f'{domain_sid}-{security.DOMAIN_RID_ADMINS}'
        else:
            expected_sid = expected_sid.format(domain_sid=domain_sid)

        owner_sid = str(descriptor.owner_sid)

        self.assertEqual(expected_sid, owner_sid)

    cases = {
        'AA': 'S-1-5-32-579',
        'AC': 'S-1-15-2-1',
        'AN': 'S-1-5-7',
        'AO': 'S-1-5-32-548',
        'AP': '{domain_sid}-525',
        'AS': 'S-1-18-1',
        'AU': 'S-1-5-11',
        'BA': 'S-1-5-32-544',
        'BG': 'S-1-5-32-546',
        'BO': 'S-1-5-32-551',
        'BU': 'S-1-5-32-545',
        'CA': '{domain_sid}-517',
        'CD': 'S-1-5-32-574',
        'CG': 'S-1-3-1',
        'CN': '{domain_sid}-522',
        'CO': 'S-1-3-0',
        'CY': 'S-1-5-32-569',
        'DC': '{domain_sid}-515',
        'DD': '{domain_sid}-516',
        'DG': '{domain_sid}-514',
        'DU': '{domain_sid}-513',
        'EA': '{domain_sid}-519',
        'ED': 'S-1-5-9',
        'EK': '{domain_sid}-527',
        'ER': 'S-1-5-32-573',
        'ES': 'S-1-5-32-576',
        'HA': 'S-1-5-32-578',
        'HI': 'S-1-16-12288',
        'IS': 'S-1-5-32-568',
        'IU': 'S-1-5-4',
        'KA': '{domain_sid}-526',
        'LA': '{domain_sid}-500',
        'LG': '{domain_sid}-501',
        'LS': 'S-1-5-19',
        'LU': 'S-1-5-32-559',
        'LW': 'S-1-16-4096',
        'ME': 'S-1-16-8192',
        'MP': 'S-1-16-8448',
        'MS': 'S-1-5-32-577',
        'MU': 'S-1-5-32-558',
        'NO': 'S-1-5-32-556',
        'NS': 'S-1-5-20',
        'NU': 'S-1-5-2',
        'OW': 'S-1-3-4',
        'PA': '{domain_sid}-520',
        'PO': 'S-1-5-32-550',
        'PS': 'S-1-5-10',
        'PU': 'S-1-5-32-547',
        'RA': 'S-1-5-32-575',
        'RC': 'S-1-5-12',
        'RD': 'S-1-5-32-555',
        'RE': 'S-1-5-32-552',
        'RM': 'S-1-5-32-580',
        'RO': '{domain_sid}-498',
        'RS': '{domain_sid}-553',
        'RU': 'S-1-5-32-554',
        'SA': '{domain_sid}-518',
        'SI': 'S-1-16-16384',
        'SO': 'S-1-5-32-549',
        'SS': 'S-1-18-2',
        'SU': 'S-1-5-6',
        'SY': 'S-1-5-18',
        # Not tested, as it always gives us an OPERATIONS_ERROR with Windows.
        # 'UD': 'S-1-5-84-0-0-0-0-0',
        'WD': 'S-1-1-0',
        'WR': 'S-1-5-33',
        'aa': 'S-1-5-32-579',
        'Aa': 'S-1-5-32-579',
        'aA': 'S-1-5-32-579',
        'BR': None,
        'IF': None,
        'LK': None,
    }


if __name__ == '__main__':
    global_asn1_print = False
    global_hexdump = False
    import unittest
    unittest.main()
