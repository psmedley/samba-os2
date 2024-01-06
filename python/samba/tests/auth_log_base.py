# Unix SMB/CIFS implementation.
# Copyright (C) Andrew Bartlett <abartlet@samba.org> 2017
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

"""Tests for the Auth and AuthZ logging.
"""

import samba.tests
from samba.messaging import Messaging
from samba.dcerpc.messaging import MSG_AUTH_LOG, AUTH_EVENT_NAME
from samba.param import LoadParm
import time
import json
import os
import re


def default_msg_filter(msg):
    # When our authentication logging tests were written, these were the only
    # supported message types. The tests were built on the assumption that no
    # new types would be added, and violating this assumption will result in
    # many tests failing as they receive messages that they weren’t
    # expecting. To allow these tests to continue to pass, this default filter
    # makes sure that only messages for which the tests are prepared pass
    # though.
    default_supported_types = {
        "Authentication",
        "Authorization",
    }

    return msg['type'] in default_supported_types


class NoMessageException(Exception):
    pass


class AuthLogTestBase(samba.tests.TestCase):

    @classmethod
    def setUpClass(cls):
        super().setUpClass()

        # connect to the server's messaging bus (we need to explicitly load a
        # different smb.conf here, because in all other respects this test
        # wants to act as a separate remote client)
        server_conf = os.getenv('SERVERCONFFILE')
        if server_conf:
            lp_ctx = LoadParm(filename_for_non_global_lp=server_conf)
        else:
            lp_ctx = samba.tests.env_loadparm()
        cls.msg_ctx = Messaging((1,), lp_ctx=lp_ctx)
        cls.msg_ctx.irpc_add_name(AUTH_EVENT_NAME)

        # Now switch back to using the client-side smb.conf. The tests will
        # use the first interface in the client.conf (we need to strip off
        # the subnet mask portion)
        lp_ctx = samba.tests.env_loadparm()
        client_ip_and_mask = lp_ctx.get('interfaces')[0]
        client_ip = client_ip_and_mask.split('/')[0]

        # the messaging ctx is the server's view of the world, so our own
        # client IP will be the remoteAddress when connections are logged
        cls.remoteAddress = client_ip

        def messageHandler(context, msgType, src, message):
            # This does not look like sub unit output and it
            # makes these tests much easier to debug.
            print(message)
            jsonMsg = json.loads(message)
            context["messages"].append(jsonMsg)

        cls.context = {"messages": []}
        cls.msg_handler_and_context = (messageHandler, cls.context)
        cls.msg_ctx.register(cls.msg_handler_and_context,
                             msg_type=MSG_AUTH_LOG)

        cls.server = os.environ["SERVER"]
        cls.connection = None

    @classmethod
    def tearDownClass(cls):
        cls.msg_ctx.deregister(cls.msg_handler_and_context,
                               msg_type=MSG_AUTH_LOG)
        cls.msg_ctx.irpc_remove_name(AUTH_EVENT_NAME)

        super().tearDownClass()

    def setUp(self):
        super(AuthLogTestBase, self).setUp()
        type(self).discardMessages()

    def isRemote(self, message):
        if self.remoteAddress is None:
            return True

        supported_types = {
            "Authentication",
            "Authorization",
            "KDC Authorization",
        }
        message_type = message["type"]
        if message_type in supported_types:
            remote = message[message_type]["remoteAddress"]
        else:
            return False

        try:
            addr = remote.split(":")
            return addr[1] == self.remoteAddress
        except IndexError:
            return False

    def waitForMessages(self, isLastExpectedMessage, connection=None, *,
                        msgFilter=default_msg_filter):
        """Wait for all the expected messages to arrive
        The connection is passed through to keep the connection alive
        until all the logging messages have been received.

        By default, only Authentication and Authorization messages will be
        returned, so that old tests continue to pass. To receive all messages,
        pass msgFilter=None.

        """

        messages = []
        while True:
            try:
                msg = self.nextMessage(msgFilter=msgFilter)
            except NoMessageException:
                return []

            messages.append(msg)
            if isLastExpectedMessage(msg):
                return messages

    def nextMessage(self, msgFilter=None):
        """Return the next relevant message, or throw a NoMessageException."""
        def is_relevant(msg):
            if not self.isRemote(msg):
                return False

            if msgFilter is None:
                return True

            return msgFilter(msg)

        messages = self.context['messages']

        while True:
            timeout = 2
            until = time.time() + timeout

            while not messages:
                # Fetch a new message from the messaging bus.

                current = time.time()
                if until < current:
                    break

                self.msg_ctx.loop_once(until - current)

            if not messages:
                raise NoMessageException('timed out looking for a message')

            # Grab the next message from the queue.
            msg = messages.pop(0)
            if is_relevant(msg):
                return msg

    # Discard any previously queued messages.
    @classmethod
    def discardMessages(cls):
        messages = cls.context["messages"]

        while True:
            messages.clear()

            # tevent presumably has other tasks to run, so we might need two or
            # three loops before a message comes through.
            for _ in range(5):
                cls.msg_ctx.loop_once(0.001)

            if not messages:
                # No new messages. We’ve probably got them all.
                break

    # Remove any NETLOGON authentication messages
    # NETLOGON is only performed once per session, so to avoid ordering
    # dependencies within the tests it's best to strip out NETLOGON messages.
    #
    def remove_netlogon_messages(self, messages):
        def is_not_netlogon(msg):
            if "Authentication" not in msg:
                return True
            sd = msg["Authentication"]["serviceDescription"]
            return sd != "NETLOGON"

        return list(filter(is_not_netlogon, messages))

    GUID_RE = re.compile(
        "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")

    #
    # Is the supplied GUID string correctly formatted
    #
    def is_guid(self, guid):
        return self.GUID_RE.fullmatch(guid)
