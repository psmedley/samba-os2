# Unix SMB/CIFS implementation.
# Copyright Volker Lendecke <vl@samba.org> 2022
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

from samba.samba3 import libsmb_samba_internal as libsmb
from samba import reparse_symlink
from samba import (ntstatus,NTSTATUSError)
from samba.dcerpc import security as sec
import samba.tests.libsmb

class Smb2SymlinkTests(samba.tests.libsmb.LibsmbTests):

    def connections(self):
        share = samba.tests.env_get_var_value(
            "SMB1_SHARE", allow_missing=True)
        if not share:
            share = "nosymlinks_smb1allow"

        try:
            smb1 = libsmb.Conn(
                self.server_ip,
                share,
                self.lp,
                self.creds)
        except NTSTATUSError as e:
            if e.args[0] != ntstatus.NT_STATUS_CONNECTION_RESET:
                raise

        share = samba.tests.env_get_var_value(
            "SMB2_SHARE", allow_missing=True)
        if not share:
            share = "nosymlinks"
        smb2 = libsmb.Conn(
            self.server_ip,
            share,
            self.lp,
            self.creds)
        return (smb1, smb2)

    def clean_file(self, conn, filename):
        try:
            conn.unlink(filename)
        except NTSTATUSError as e:
            if e.args[0] != ntstatus.NT_STATUS_OBJECT_NAME_NOT_FOUND:
                raise

    def create_symlink(self, conn, target, symlink):
        self.clean_file(conn, symlink)
        if (conn.protocol() < libsmb.PROTOCOL_SMB2_02 and conn.have_posix()):
            conn.smb1_symlink(target, symlink)
        else:
            flags = 0 if target[0]=='/' else 1
            syml = conn.create(
                symlink,
                DesiredAccess=sec.SEC_FILE_READ_ATTRIBUTE|
                sec.SEC_FILE_WRITE_ATTRIBUTE|
                sec.SEC_STD_DELETE,
                FileAttributes=libsmb.FILE_ATTRIBUTE_NORMAL,
                CreateDisposition=libsmb.FILE_OPEN_IF,
                CreateOptions=libsmb.FILE_OPEN_REPARSE_POINT)
            b = reparse_symlink.symlink_put(target, target, 0, 1)
            conn.fsctl(syml, libsmb.FSCTL_SET_REPARSE_POINT, b, 0)
            conn.close(syml)

    def assert_symlink_exception(self, e, expect):
        self.assertEqual(e.args[0], ntstatus.NT_STATUS_STOPPED_ON_SYMLINK)
        for k,v in expect.items():
            if (k == "flags"):
                # Ignore symlink trust flags for now
                expected = v & ~libsmb.SYMLINK_TRUST_MASK
                got = e.args[2].get(k) & ~libsmb.SYMLINK_TRUST_MASK
                self.assertEqual((k,got), (k,expected))
            else:
                self.assertEqual((k,e.args[2].get(k)), (k,v))

    def test_symlinkerror_directory(self):
        """Test a symlink in a nonterminal path component"""
        (smb1,smb2) = self.connections()
        symlink="syml"
        target="foo"
        suffix="bar"

        self.create_symlink(smb1, target, symlink);

        with self.assertRaises(NTSTATUSError) as e:
            fd = smb2.create_ex(f'{symlink}\\{suffix}')

        self.assert_symlink_exception(
            e.exception,
            { 'unparsed_path_length' : len(suffix)+1,
              'substitute_name' : target,
              'print_name' : target,
              'flags' : 0x20000001 })

        self.clean_file(smb1, symlink)

    def test_symlinkerror_file(self):
        """Test a simple symlink in a terminal path"""
        (smb1,smb2) = self.connections()
        symlink="syml"
        target="foo"

        self.create_symlink(smb1, target, symlink);

        with self.assertRaises(NTSTATUSError) as e:
            fd = smb2.create_ex(f'{symlink}')

        self.assert_symlink_exception(
                e.exception,
            { 'unparsed_path_length' : 0,
              'substitute_name' : target,
              'print_name' : target,
              'flags' : 0x20000001 })

        self.clean_file(smb1, symlink)

    def test_symlinkerror_absolute_outside_share(self):
        """
        Test symlinks to outside of the share
        We return the contents 1:1
        """
        (smb1,smb2) = self.connections()
        symlink="syml"

        for target in ["/etc", "//foo/bar", "/"]:

            self.create_symlink(smb1, target, symlink)

            with self.assertRaises(NTSTATUSError) as e:
                fd = smb2.create_ex(f'{symlink}')

            self.assert_symlink_exception(
                e.exception,
                { 'unparsed_path_length' : 0,
                  'substitute_name' : target,
                  'print_name' : target,
                  'flags' : 0 })

            self.clean_file(smb1, symlink)

    def test_symlinkerror_absolute_inshare(self):
        """Test an absolute symlink inside the share"""
        (smb1,smb2) = self.connections()
        symlink="syml"

        localpath=samba.tests.env_get_var_value("LOCAL_PATH")
        shareroot=f'{localpath}/nosymlinks'
        rel_dest="dst"
        target=f'{shareroot}/{rel_dest}'

        self.create_symlink(smb1, target, symlink)

        with self.assertRaises(NTSTATUSError) as e:
            fd = smb2.create_ex(f'{symlink}')

        self.assert_symlink_exception(
            e.exception,
            { 'unparsed_path_length' : 0,
              'substitute_name' : rel_dest,
              'print_name' : rel_dest,
              'flags' : 0 })

        self.clean_file(smb1, symlink)

if __name__ == '__main__':
    import unittest
    unittest.main()
