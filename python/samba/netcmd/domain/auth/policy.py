# Unix SMB/CIFS implementation.
#
# authentication silos - authentication policy management
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
from samba.netcmd.domain.models import AuthenticationPolicy
from samba.netcmd.domain.models.auth_policy import MIN_TGT_LIFETIME,\
    MAX_TGT_LIFETIME, StrongNTLMPolicy
from samba.netcmd.domain.models.exceptions import ModelError
from samba.netcmd.validators import Range


class cmd_domain_auth_policy_list(Command):
    """List authentication policies on the domain."""

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

        # Authentication policies grouped by cn.
        try:
            policies = {policy.cn: policy.as_dict()
                        for policy in AuthenticationPolicy.query(ldb)}
        except ModelError as e:
            raise CommandError(e)

        # Using json output format gives more detail.
        if output_format == "json":
            self.print_json(policies)
        else:
            for policy in policies.keys():
                self.outf.write(f"{policy}\n")


class cmd_domain_auth_policy_view(Command):
    """View an authentication policy on the domain."""

    synopsis = "%prog -H <URL> [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", "--URL", help="LDB URL for database or target server.",
               type=str, metavar="URL", dest="ldap_url"),
        Option("--name",
               help="Name of authentication policy to view (required).",
               dest="name", action="store", type=str),
    ]

    def run(self, ldap_url=None, sambaopts=None, credopts=None, name=None):

        if not name:
            raise CommandError("Argument --name is required.")

        ldb = self.ldb_connect(ldap_url, sambaopts, credopts)

        try:
            policy = AuthenticationPolicy.get(ldb, cn=name)
        except ModelError as e:
            raise CommandError(e)

        # Check if authentication policy exists first.
        if policy is None:
            raise CommandError(f"Authentication policy {name} not found.")

        # Display policy as JSON.
        self.print_json(policy.as_dict())


class cmd_domain_auth_policy_create(Command):
    """Create an authentication policy on the domain."""

    synopsis = "%prog -H <URL> [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", "--URL", help="LDB URL for database or target server.",
               type=str, metavar="URL", dest="ldap_url"),
        Option("--name", help="Name of authentication policy (required).",
               dest="name", action="store", type=str),
        Option("--description",
               help="Optional description for authentication policy.",
               dest="description", action="store", type=str),
        Option("--protect",
               help="Protect authentication silo from accidental deletion.",
               dest="protect", action="store_true"),
        Option("--unprotect",
               help="Unprotect authentication silo from accidental deletion.",
               dest="unprotect", action="store_true"),
        Option("--audit",
               help="Only audit authentication policy.",
               dest="audit", action="store_true"),
        Option("--enforce",
               help="Enforce authentication policy.",
               dest="enforce", action="store_true"),
        Option("--strong-ntlm-policy",
               help=f"Strong NTLM Policy ({StrongNTLMPolicy.choices_str()}).",
               dest="strong_ntlm_policy", type="choice", action="store",
               choices=StrongNTLMPolicy.get_choices(),
               default="Disabled"),
        Option("--user-tgt-lifetime",
               help="Ticket-Granting-Ticket lifetime for user accounts.",
               dest="user_tgt_lifetime", type=int, action="store",
               validators=[Range(min=MIN_TGT_LIFETIME, max=MAX_TGT_LIFETIME)]),
        Option("--user-allow-ntlm-auth",
               help="Allow NTLM network authentication when user "
                    "is restricted to selected devices.",
               dest="user_allow_ntlm_auth", action="store_true",
               default=False),
        Option("--service-tgt-lifetime",
               help="Ticket-Granting-Ticket lifetime for service accounts.",
               dest="service_tgt_lifetime", type=int, action="store",
               validators=[Range(min=MIN_TGT_LIFETIME, max=MAX_TGT_LIFETIME)]),
        Option("--service-allow-ntlm-auth",
               help="Allow NTLM network authentication when service "
                    "is restricted to selected devices.",
               dest="service_allow_ntlm_auth", action="store_true",
               default=False),
        Option("--computer-tgt-lifetime",
               help="Ticket-Granting-Ticket lifetime for computer accounts.",
               dest="computer_tgt_lifetime", type=int, action="store",
               validators=[Range(min=MIN_TGT_LIFETIME, max=MAX_TGT_LIFETIME)]),
    ]

    def run(self, ldap_url=None, sambaopts=None, credopts=None, name=None,
            description=None, protect=None, unprotect=None, audit=None,
            enforce=None, strong_ntlm_policy=None, user_tgt_lifetime=None,
            user_allow_ntlm_auth=None, service_tgt_lifetime=None,
            service_allow_ntlm_auth=None, computer_tgt_lifetime=None):

        if not name:
            raise CommandError("Argument --name is required.")
        if protect and unprotect:
            raise CommandError("--protect and --unprotect cannot be used together.")
        if audit and enforce:
            raise CommandError("--audit and --enforce cannot be used together.")

        ldb = self.ldb_connect(ldap_url, sambaopts, credopts)

        try:
            policy = AuthenticationPolicy.get(ldb, cn=name)
        except ModelError as e:
            raise CommandError(e)

        # Make sure authentication policy doesn't already exist.
        if policy is not None:
            raise CommandError(f"Authentication policy {name} already exists.")

        # New policy object.
        policy = AuthenticationPolicy(
            cn=name,
            description=description,
            strong_ntlm_policy=StrongNTLMPolicy[strong_ntlm_policy.upper()],
            user_allow_ntlm_auth=user_allow_ntlm_auth,
            user_tgt_lifetime=user_tgt_lifetime,
            service_allow_ntlm_auth=service_allow_ntlm_auth,
            service_tgt_lifetime=service_tgt_lifetime,
            computer_tgt_lifetime=computer_tgt_lifetime,
        )

        # Either --enforce will be set or --audit but never both.
        # The default if both are missing is enforce=True.
        if enforce is not None:
            policy.enforced = enforce
        else:
            policy.enforced = not audit

        # Create policy.
        try:
            policy.save(ldb)

            if protect:
                policy.protect(ldb)
        except ModelError as e:
            raise CommandError(e)

        # Authentication policy created successfully.
        self.outf.write(f"Created authentication policy: {name}\n")


class cmd_domain_auth_policy_modify(Command):
    """Modify authentication policies on the domain."""

    synopsis = "%prog -H <URL> [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", "--URL", help="LDB URL for database or target server.",
               type=str, metavar="URL", dest="ldap_url"),
        Option("--name", help="Name of authentication policy (required).",
               dest="name", action="store", type=str),
        Option("--description",
               help="Optional description for authentication policy.",
               dest="description", action="store", type=str),
        Option("--protect",
               help="Protect authentication silo from accidental deletion.",
               dest="protect", action="store_true"),
        Option("--unprotect",
               help="Unprotect authentication silo from accidental deletion.",
               dest="unprotect", action="store_true"),
        Option("--audit",
               help="Only audit authentication policy.",
               dest="audit", action="store_true"),
        Option("--enforce",
               help="Enforce authentication policy.",
               dest="enforce", action="store_true"),
        Option("--strong-ntlm-policy",
               help=f"Strong NTLM Policy ({StrongNTLMPolicy.choices_str()}).",
               dest="strong_ntlm_policy", type="choice", action="store",
               choices=StrongNTLMPolicy.get_choices()),
        Option("--user-tgt-lifetime",
               help="Ticket-Granting-Ticket lifetime for user accounts.",
               dest="user_tgt_lifetime", type=int, action="store",
               validators=[Range(min=MIN_TGT_LIFETIME, max=MAX_TGT_LIFETIME)]),
        Option("--user-allow-ntlm-auth",
               help="Allow NTLM network authentication when user "
                    "is restricted to selected devices.",
               dest="user_allow_ntlm_auth", action="store_true",
               default=False),
        Option("--service-tgt-lifetime",
               help="Ticket-Granting-Ticket lifetime for service accounts.",
               dest="service_tgt_lifetime", type=int, action="store",
               validators=[Range(min=MIN_TGT_LIFETIME, max=MAX_TGT_LIFETIME)]),
        Option("--service-allow-ntlm-auth",
               help="Allow NTLM network authentication when service "
                    "is restricted to selected devices.",
               dest="service_allow_ntlm_auth", action="store_true",
               default=False),
        Option("--computer-tgt-lifetime",
               help="Ticket-Granting-Ticket lifetime for computer accounts.",
               dest="computer_tgt_lifetime", type=int, action="store",
               validators=[Range(min=MIN_TGT_LIFETIME, max=MAX_TGT_LIFETIME)]),
    ]

    def run(self, ldap_url=None, sambaopts=None, credopts=None, name=None,
            description=None, protect=None, unprotect=None, audit=None,
            enforce=None, strong_ntlm_policy=None, user_tgt_lifetime=None,
            user_allow_ntlm_auth=None, service_tgt_lifetime=None,
            service_allow_ntlm_auth=None, computer_tgt_lifetime=None):

        if not name:
            raise CommandError("Argument --name is required.")
        if protect and unprotect:
            raise CommandError("--protect and --unprotect cannot be used together.")
        if audit and enforce:
            raise CommandError("--audit and --enforce cannot be used together.")

        ldb = self.ldb_connect(ldap_url, sambaopts, credopts)

        try:
            policy = AuthenticationPolicy.get(ldb, cn=name)
        except ModelError as e:
            raise CommandError(e)

        # Check if authentication policy exists.
        if policy is None:
            raise CommandError(f"Authentication policy {name} not found.")

        # Either --enforce will be set or --audit but never both.
        if enforce:
            policy.enforced = True
        elif audit:
            policy.enforced = False

        # Update the description.
        if description is not None:
            policy.description = description

        # User sign on
        ###############

        if strong_ntlm_policy is not None:
            policy.strong_ntlm_policy = \
                StrongNTLMPolicy[strong_ntlm_policy.upper()]

        if user_tgt_lifetime is not None:
            policy.user_tgt_lifetime = user_tgt_lifetime

        # Service sign on
        ##################

        if service_tgt_lifetime is not None:
            policy.service_tgt_lifetime = service_tgt_lifetime

        # Computer
        ###########

        if computer_tgt_lifetime is not None:
            policy.computer_tgt_lifetime = computer_tgt_lifetime

        # Update policy.
        try:
            policy.save(ldb)

            if protect:
                policy.protect(ldb)
            elif unprotect:
                policy.unprotect(ldb)
        except ModelError as e:
            raise CommandError(e)

        # Authentication policy updated successfully.
        self.outf.write(f"Updated authentication policy: {name}\n")


class cmd_domain_auth_policy_delete(Command):
    """Delete authentication policies on the domain."""

    synopsis = "%prog -H <URL> [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", "--URL", help="LDB URL for database or target server.",
               type=str, metavar="URL", dest="ldap_url"),
        Option("--name", help="Name of authentication policy (required).",
               dest="name", action="store", type=str),
        Option("--force", help="Force delete protected authentication policy.",
               dest="force", action="store_true")
    ]

    def run(self, ldap_url=None, sambaopts=None, credopts=None, name=None,
            force=None):

        if not name:
            raise CommandError("Argument --name is required.")

        ldb = self.ldb_connect(ldap_url, sambaopts, credopts)

        try:
            policy = AuthenticationPolicy.get(ldb, cn=name)
        except ModelError as e:
            raise CommandError(e)

        # Check if authentication policy exists first.
        if policy is None:
            raise CommandError(f"Authentication policy {name} not found.")

        # Delete item, --force removes delete protection first.
        try:
            if force:
                policy.unprotect(ldb)

            policy.delete(ldb)
        except ModelError as e:
            if not force:
                raise CommandError(
                    f"{e}\nTry --force to delete protected authentication policies.")
            else:
                raise CommandError(e)

        # Authentication policy deleted successfully.
        self.outf.write(f"Deleted authentication policy: {name}\n")


class cmd_domain_auth_policy(SuperCommand):
    """Manage authentication policies on the domain."""

    subcommands = {
        "list": cmd_domain_auth_policy_list(),
        "view": cmd_domain_auth_policy_view(),
        "create": cmd_domain_auth_policy_create(),
        "modify": cmd_domain_auth_policy_modify(),
        "delete": cmd_domain_auth_policy_delete(),
    }
