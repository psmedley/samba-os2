#!/usr/bin/env bash

# Verify that 'ctdb ban' causes a node to yield the leader role

. "${TEST_SCRIPTS_DIR}/integration.bash"

set -e

ctdb_test_init

# This is the node used to execute commands
select_test_node
echo

# test_node set by select_test_node()
# shellcheck disable=SC2154
leader_get "$test_node"

# leader set by leader_get()
# shellcheck disable=SC2154
echo "Removing leader capability from leader ${leader}..."
ctdb_onnode "$test_node" setleaderrole off -n "$leader"

wait_until_leader_has_changed "$test_node"
