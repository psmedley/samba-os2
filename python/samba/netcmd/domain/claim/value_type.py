# Unix SMB/CIFS implementation.
#
# claim value type management
#
# Copyright (C) Catalyst.Net Ltd. 2023
#
# Written by Rob van der Linde <rob@catalyst.net.nz>
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

import samba.getopt as options
from samba.netcmd import Command, CommandError, Option, SuperCommand
from samba.netcmd.domain.models import ValueType
from samba.netcmd.domain.models.exceptions import ModelError


class cmd_domain_claim_value_type_list(Command):
    """List claim values types on the domain."""

    synopsis = "%prog -H <URL> [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", "--URL", help="LDB URL for database or target server.",
               type=str, metavar="URL", dest="ldap_url"),
        Option("--json", help="Output results in JSON format.",
               dest="output_format", action="store_const", const="json"),
    ]

    def run(self, ldap_url=None, sambaopts=None, credopts=None,
            output_format=None):

        ldb = self.ldb_connect(ldap_url, sambaopts, credopts)

        # Value types grouped by display name.
        try:
            value_types = {value_type.display_name: value_type.as_dict()
                           for value_type in ValueType.query(ldb)}
        except ModelError as e:
            raise CommandError(e)

        # Using json output format gives more detail.
        if output_format == "json":
            self.print_json(value_types)
        else:
            for value_type in value_types.keys():
                self.outf.write(f"{value_type}\n")


class cmd_domain_claim_value_type_view(Command):
    """View a single claim value type on the domain."""

    synopsis = "%prog -H <URL> [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", "--URL", help="LDB URL for database or target server.",
               type=str, metavar="URL", dest="ldap_url"),
        Option("--name",
               help="Display name of claim value type to view (required).",
               dest="name", action="store", type=str),
    ]

    def run(self, ldap_url=None, sambaopts=None, credopts=None, name=None):

        if not name:
            raise CommandError("Argument --name is required.")

        ldb = self.ldb_connect(ldap_url, sambaopts, credopts)

        try:
            value_type = ValueType.get(ldb, display_name=name)
        except ModelError as e:
            raise CommandError(e)

        # Check if value type exists first.
        if value_type is None:
            raise CommandError(f"Value type {name} not found.")

        # Display vale type as JSON.
        self.print_json(value_type.as_dict())


class cmd_domain_claim_value_type(SuperCommand):
    """Manage claim value types on the domain."""

    subcommands = {
        "list": cmd_domain_claim_value_type_list(),
        "view": cmd_domain_claim_value_type_view(),
    }
