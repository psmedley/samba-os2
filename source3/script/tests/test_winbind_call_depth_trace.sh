#!/bin/sh

# Copyright (c) Pavel Filipenský <pfilipensky@samba.org>
# License: GPLv3

if [ $# -lt 4 ]; then
	echo "Usage: test_winbind_call_depth_trace SMBCONTROL CONFIGURATION PREFIX TESTENV"
	exit 1
fi

SMBCONTROL="${1}"
CONFIGURATION=${2}
PREFIX="${3}"
TESTENV="${4}"
shift 4

incdir=$(dirname "$0")/../../../testprogs/blackbox
. "$incdir"/subunit.sh

failed=0

PREFIX_ABS="$(readlink -f "${PREFIX}")"
# Strip from TESTENV the ':local' if present
TESTENV_SUBDIR=${TESTENV%:*}

LOGFILE="${PREFIX_ABS}/${TESTENV_SUBDIR}/logs/log.winbindd"
# Add support for "WINBINDD_DONT_LOG_STDOUT=1"
if [ ! -r "${LOGFILE}" ]; then
	TEST_LOGFILE="${PREFIX_ABS}/${TESTENV_SUBDIR}/winbindd_test.log"
	subunit_start_test "test winbind call depth trace"
	subunit_skip_test "test winbind call depth trace" <<EOF
Test is skipped, we need $LOGFILE but have only $TEST_LOGFILE which is missing debug headers (they are not printed to stdout).
EOF
	exit 0
fi

saved_level=1

get_winbind_loglevel()
{
	s1=$(${SMBCONTROL} "${CONFIGURATION}" winbind debuglevel)
	# We need to get the all level from output like this:
	# "PID 664474: all:1 tdb:1 printdrivers:1  lanman:1 smb:1 rpc_parse:1 rpc_srv:1 rpc_cli:1 passdb:1 sam:1..."
	# 1. remove PID 664474:
	s2=${s1#PID*: }
	# "all:1 tdb:1 printdrivers:1  lanman:1 smb:1 rpc_parse:1 rpc_srv:1 rpc_cli:1 passdb"
	# 2. remove " tdb:1 printdrivers:1 ..."
	s3=${s2%% *}
	# "all:1"
	# 3. remove "all:"
	saved_level=${s3#all:}
}

# Example of trace line
# [2023/01/25 00:20:33.307038,  5, pid=535581, effective(0, 0), real(0, 0), class=winbind, traceid=78, depth=4] ../../source3/winbindd/wb_group_members.c:310(wb_group_members_send)
test_winbind_call_depth_trace()
{
	get_winbind_loglevel

	# If loglevel < 10, set it to 10.
	if [ "$saved_level" -lt 10 ]; then
		${SMBCONTROL}  "${CONFIGURATION}" winbind debug 10
	fi

	COUNT1=$(grep -c wb_group_members_send "$LOGFILE")

	id ADDOMAIN/alice
	ret=$?

	# Restore loglevel, if it was changed.
	if [ "$saved_level" -lt 10 ]; then
		${SMBCONTROL} "${CONFIGURATION}" winbind debug "$saved_level"
	fi

	if [ $ret != 0 ]; then
		echo "Command 'id ADDOMAIN/alice' failed!"
		return 1
	fi

	# Check that there are more lines with wb_group_members_send
	COUNT2=$(grep -c wb_group_members_send "$LOGFILE")
	if [ "$COUNT1" -eq "$COUNT2" ]; then
		echo "The number of the trace lines in $LOGFILE has not increased."
		return 1
	fi

	# Test that the depth of last line with 'wb_group_members_send' is: depth=4
	COUNT3=$(grep wb_group_members_send "$LOGFILE" | tail -1 | grep -c depth=4)
	if [ "$COUNT3" -ne 1 ]; then
		echo "The last line with wb_group_members_send should have depth=4."
		return 1
	fi

	# Test that the indentation of the line below last 'wb_group_members_send' is indented by 2+4*4 spaces:
	COUNT4=$(grep -A1 wb_group_members_send "$LOGFILE" | tail -1| grep -c '^                  WB command group_members start')
	if [ "$COUNT4" -ne 1 ]; then
		echo "The line after the last line with wb_group_members_send should be indented by 18 spaces."
		return 1
	fi

	return 0
}

case ${TESTENV} in
ad_member*)
	;;
*)
	echo "Test is for ad_member only, but called for ${TESTENV}." | subunit_fail_test "test winbind call depth trace"
	exit 1;
esac

testit "test winbind call depth trace"  test_winbind_call_depth_trace || failed=$((failed + 1))
testok "$0" "$failed"
