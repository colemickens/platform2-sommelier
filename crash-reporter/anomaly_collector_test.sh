#! /bin/bash
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test for warn_collector.  Run the warn collector in the background, emulate
# the kernel by appending lines to the log file "messages", and observe the log
# of the (fake) crash reporter each time is run by the warn collector daemon.

set -e

fail() {
  printf '[ FAIL ] %b\n' "$*"
  exit 1
}

cleanup() {
  # Kill daemon (if started) on exit
  kill %1 || true
  rm -rf "${OUT}"
}

check_log() {
  local n_expected=$1
  if [[ ! -f ${TESTLOG} ]]; then
    fail "${TESTLOG} was not created"
  fi
  if [[ $(wc -l < "${TESTLOG}") -ne ${n_expected} ]]; then
    fail "expected ${n_expected} lines in ${TESTLOG}, found this instead:
$(<"${TESTLOG}")"
  fi
  if egrep -qv '^[0-9a-f]{8}' "${TESTLOG}"; then
    fail "found bad lines in ${TESTLOG}:
$(<"${TESTLOG}")"
  fi
}

trap cleanup EXIT

SRC="$(readlink -f "$(dirname "$0")")"
PATH="${SRC}:${PATH}"
echo "Testing: $(which warn_collector)"

OUT="$(mktemp -dt warn_collector_test_XXXXXXXXXX)"
cd "${OUT}"
TESTLOG="${OUT}/warn-test-log"

cp "${SRC}/warn_collector_test_reporter.sh" .
touch messages

# Start the collector daemon.  With the --test option, the daemon reads input
# from ./messages, writes the warning into ./warning, and invokes
# ./warn_collector_test_reporter.sh to report the warning.
warn_collector --test &
sleep 1

# Emit a warning to messages and check that it is collected.
cat "${SRC}/TEST_WARNING" >> messages
sleep 1
check_log 1

# Add the same warning to messages, verify that it is NOT collected
cat "${SRC}/TEST_WARNING" >> messages
sleep 1
check_log 1

# Add a slightly different warning to messages, check that it is collected.
sed s/ttm_bo_vm.c/file_one.c/ < "${SRC}/TEST_WARNING" >> messages
sleep 1
check_log 2

# Emulate log rotation, add a warning, and check.
mv messages messages.1
sed s/ttm_bo_vm.c/file_two.c/ < "${SRC}/TEST_WARNING" >> messages
sleep 2
check_log 3

# Emit a warning in old format and check that it is collected.
cat "${SRC}/TEST_WARNING_OLD" >> messages
sleep 1
check_log 4

# Success!
exit 0
