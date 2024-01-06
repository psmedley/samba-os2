# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2009-2012
# Copyright (C) Theresa Halloran <theresahalloran@gmail.com> 2011
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

import json
import optparse
import sys
import textwrap
import traceback

import samba
from ldb import ERR_INVALID_CREDENTIALS, LdbError
from samba import colour
from samba.auth import system_session
from samba.getopt import SambaOption, OptionError
from samba.logger import get_samba_logger
from samba.samdb import SamDB

from .encoders import JSONEncoder
from .validators import ValidationError


class Option(SambaOption):
    ATTRS = SambaOption.ATTRS + ["validators"]
    SUPPRESS_HELP = optparse.SUPPRESS_HELP

    def run_validators(self, opt, value):
        """Runs the list of validators on the current option.

        If the validator raises ValidationError, turn that into CommandError
        which gives nicer output.
        """
        validators = getattr(self, "validators") or []

        for validator in validators:
            try:
                validator(opt, value)
            except ValidationError as e:
                raise CommandError(e)

    def convert_value(self, opt, value):
        """Override convert_value to run validators just after.

        This can also be done in process() but there we would have to
        replace the entire method.
        """
        value = super().convert_value(opt, value)
        self.run_validators(opt, value)
        return value


# This help formatter does text wrapping and preserves newlines


class PlainHelpFormatter(optparse.IndentedHelpFormatter):
    def format_description(self, description=""):
        desc_width = self.width - self.current_indent
        indent = " " * self.current_indent
        paragraphs = description.split('\n')
        wrapped_paragraphs = [
            textwrap.fill(p,
                          desc_width,
                          initial_indent=indent,
                          subsequent_indent=indent)
            for p in paragraphs]
        result = "\n".join(wrapped_paragraphs) + "\n"
        return result

    def format_epilog(self, epilog):
        if epilog:
            return "\n" + epilog + "\n"
        else:
            return ""


class Command(object):
    """A samba-tool command."""

    def _get_short_description(self):
        return self.__doc__.splitlines()[0].rstrip("\n")

    short_description = property(_get_short_description)

    def _get_full_description(self):
        lines = self.__doc__.split("\n")
        return lines[0] + "\n" + textwrap.dedent("\n".join(lines[1:]))

    full_description = property(_get_full_description)

    def _get_name(self):
        name = self.__class__.__name__
        if name.startswith("cmd_"):
            return name[4:]
        return name

    name = property(_get_name)

    # synopsis must be defined in all subclasses in order to provide the
    # command usage
    synopsis = None
    takes_args = []
    takes_options = []
    takes_optiongroups = {}

    hidden = False
    use_colour = True
    requested_colour = None

    raw_argv = None
    raw_args = None
    raw_kwargs = None

    def _set_files(self, outf=None, errf=None):
        if outf is not None:
            self.outf = outf
        if errf is not None:
            self.errf = errf

    def __init__(self, outf=sys.stdout, errf=sys.stderr):
        self._set_files(outf, errf)

    def usage(self, prog=None):
        parser, _ = self._create_parser(prog)
        parser.print_usage()

    def _print_error(self, msg, evalue=None, klass=None):
        err = colour.c_DARK_RED("ERROR")
        klass = '' if klass is None else f'({klass})'

        if evalue is None:
            print(f"{err}{klass}: {msg}", file=self.errf)
        else:
            print(f"{err}{klass}: {msg} - {evalue}", file=self.errf)

    def ldb_connect(self, ldap_url, sambaopts, credopts):
        """Helper to connect to Ldb database using command line opts."""
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)
        return SamDB(ldap_url, credentials=creds,
                     session_info=system_session(lp), lp=lp)

    def print_json(self, data):
        """Print json on the screen using consistent formatting and sorting.

        A custom JSONEncoder class is used to help with serializing unknown
        objects such as Dn for example.
        """
        json.dump(data, self.outf, cls=JSONEncoder, indent=2, sort_keys=True)
        self.outf.write("\n")

    def show_command_error(self, e):
        """display a command error"""
        if isinstance(e, CommandError):
            (etype, evalue, etraceback) = e.exception_info
            inner_exception = e.inner_exception
            message = e.message
            force_traceback = False
        else:
            (etype, evalue, etraceback) = sys.exc_info()
            inner_exception = e
            message = "uncaught exception"
            force_traceback = True

        if isinstance(e, OptionError):
            print(evalue, file=self.errf)
            self.usage()
            force_traceback = False

        elif isinstance(inner_exception, LdbError):
            (ldb_ecode, ldb_emsg) = inner_exception.args
            if ldb_ecode == ERR_INVALID_CREDENTIALS:
                print("Invalid username or password", file=self.errf)
                force_traceback = False
            elif ldb_emsg == 'LDAP client internal error: NT_STATUS_NETWORK_UNREACHABLE':
                print("Could not reach remote server", file=self.errf)
                force_traceback = False
            elif ldb_emsg.startswith("Unable to open tdb "):
                self._print_error(message, ldb_emsg, 'ldb')
                force_traceback = False
            else:
                self._print_error(message, ldb_emsg, 'ldb')

        elif isinstance(inner_exception, AssertionError):
            self._print_error(message, klass='assert')
            force_traceback = True
        elif isinstance(inner_exception, RuntimeError):
            self._print_error(message, evalue, 'runtime')
        elif type(inner_exception) is Exception:
            self._print_error(message, evalue, 'exception')
            force_traceback = True
        elif inner_exception is None:
            self._print_error(message)
        else:
            self._print_error(message, evalue, str(etype))

        if force_traceback or samba.get_debug_level() >= 3:
            traceback.print_tb(etraceback, file=self.errf)

    def _create_parser(self, prog=None, epilog=None):
        parser = optparse.OptionParser(
            usage=self.synopsis,
            description=self.full_description,
            formatter=PlainHelpFormatter(),
            prog=prog, epilog=epilog)
        parser.add_options(self.takes_options)
        optiongroups = {}
        for name in sorted(self.takes_optiongroups.keys()):
            optiongroup = self.takes_optiongroups[name]
            optiongroups[name] = optiongroup(parser)
            parser.add_option_group(optiongroups[name])
        if self.use_colour:
            parser.add_option("--color",
                              help="use colour if available (default: auto)",
                              metavar="always|never|auto",
                              default="auto")

        return parser, optiongroups

    def message(self, text):
        self.outf.write(text + "\n")

    def _resolve(self, path, *argv, outf=None, errf=None):
        """This is a leaf node, the command that will actually run."""
        self._set_files(outf, errf)
        self.command_name = path
        return (self, argv)

    def _run(self, *argv):
        parser, optiongroups = self._create_parser(self.command_name)
        opts, args = parser.parse_args(list(argv))
        # Filter out options from option groups
        kwargs = dict(opts.__dict__)
        for option_group in parser.option_groups:
            for option in option_group.option_list:
                if option.dest is not None:
                    del kwargs[option.dest]
        kwargs.update(optiongroups)

        if self.use_colour:
            self.apply_colour_choice(kwargs.pop('color', 'auto'))

        # Check for a min a max number of allowed arguments, whenever possible
        # The suffix "?" means zero or one occurrence
        # The suffix "+" means at least one occurrence
        # The suffix "*" means zero or more occurrences
        min_args = 0
        max_args = 0
        undetermined_max_args = False
        for i, arg in enumerate(self.takes_args):
            if arg[-1] != "?" and arg[-1] != "*":
                min_args += 1
            if arg[-1] == "+" or arg[-1] == "*":
                undetermined_max_args = True
            else:
                max_args += 1
        if (len(args) < min_args) or (not undetermined_max_args and len(args) > max_args):
            parser.print_usage()
            return -1

        self.raw_argv = list(argv)
        self.raw_args = args
        self.raw_kwargs = kwargs

        try:
            return self.run(*args, **kwargs)
        except Exception as e:
            self.show_command_error(e)
            return -1

    def run(self, *args, **kwargs):
        """Run the command. This should be overridden by all subclasses."""
        raise NotImplementedError(f"'{self.command_name}' run method not implemented")

    def get_logger(self, name="", verbose=False, quiet=False, **kwargs):
        """Get a logger object."""
        return get_samba_logger(
            name=name or self.name, stream=self.errf,
            verbose=verbose, quiet=quiet,
            **kwargs)

    def apply_colour_choice(self, requested):
        """Heuristics to work out whether the user wants colour output, from a
        --color=yes|no|auto option. This alters the ANSI 16 bit colour
        "constants" in the colour module to be either real colours or empty
        strings.
        """
        self.requested_colour = requested
        try:
            colour.colour_if_wanted(self.outf,
                                    self.errf,
                                    hint=requested)
        except ValueError as e:
            raise CommandError(f"Unknown --color option: {requested} "
                               "please choose from always|never|auto")


class SuperCommand(Command):
    """A samba-tool command with subcommands."""

    synopsis = "%prog <subcommand>"

    subcommands = {}

    def _resolve(self, path, *args, outf=None, errf=None):
        """This is an internal node. We need to consume one of the args and
        find the relevant child, returning an instance of that Command.

        If there are no children, this SuperCommand will be returned
        and its _run() will do a --help like thing.
        """
        self.command_name = path
        self._set_files(outf, errf)

        # We collect up certain option arguments and pass them to the
        # leaf, which is why we iterate over args, though we really
        # expect to return in the first iteration.
        deferred_args = []

        for i, a in enumerate(args):
            if a in self.subcommands:
                sub_args = args[i + 1:] + tuple(deferred_args)
                sub_path = f'{path} {a}'

                sub = self.subcommands[a]
                return sub._resolve(sub_path, *sub_args, outf=outf, errf=errf)

            elif a in [ '--help', 'help', None, '-h', '-V', '--version' ]:
                # we pass these to the leaf node.
                if a == 'help':
                    a = '--help'
                deferred_args.append(a)
                continue

            # they are talking nonsense
            print("%s: no such subcommand: %s\n" % (path, a), file=self.outf)
            return (self, [])

        # We didn't find a subcommand, but maybe we found e.g. --version
        print("%s: missing subcommand\n" % (path), file=self.outf)
        return (self, deferred_args)

    def _run(self, *argv):
        epilog = "\nAvailable subcommands:\n"

        subcmds = sorted(self.subcommands.keys())
        max_length = max([len(c) for c in subcmds])
        for cmd_name in subcmds:
            cmd = self.subcommands[cmd_name]
            if cmd.hidden:
                continue
            epilog += "  %*s  - %s\n" % (
                -max_length, cmd_name, cmd.short_description)

        epilog += ("For more help on a specific subcommand, please type: "
                   f"{self.command_name} <subcommand> (-h|--help)\n")

        parser, optiongroups = self._create_parser(self.command_name, epilog=epilog)
        opts, args = parser.parse_args(list(argv))

        # note: if argv had --help, parser.parse_args() will have
        # already done the .print_help() and attempted to exit with
        # return code 0, so we won't get here.
        parser.print_help()
        return -1


class CommandError(Exception):
    """An exception class for samba-tool Command errors."""

    def __init__(self, message, inner_exception=None):
        self.message = message
        self.inner_exception = inner_exception
        self.exception_info = sys.exc_info()

    def __repr__(self):
        return "CommandError(%s)" % self.message
