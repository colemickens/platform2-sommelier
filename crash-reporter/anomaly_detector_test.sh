#! /bin/bash
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test for anomaly_detector.  Run the anomaly detector in the background,
# emulate the kernel by appending lines to the log file "messages", and observe
# the log of the (fake) crash reporter each time is run by the anomaly detector
# daemon.

set -e

fail() {
  printf '[ FAIL ] %b\n' "$*"
  exit 1
}

cleanup() {
  # Kill daemon (if started) on exit
  kill %1 || true
  # Give the daemon a chance to exit cleanly.
  sleep 1
  rm -rf "${OUT}"
}

check_log() {
  local n_expected=$1
  local pattern=$2

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
  if [[ ! -z "${pattern}" ]] &&
     tail -n1 "${TESTLOG}" | egrep -qv "${pattern}"; then
    fail "pattern ${pattern} not found at end of ${TESTLOG}:
$(<"${TESTLOG}")"
  fi
}

# Executes "cleanup" when exiting normally (i.e. not from a signal).
# The exit code is saved and restored around execution of "cleanup"
trap cleanup EXIT

SRC="$(readlink -f "$(dirname "$0")")"
PATH="${SRC}:${PATH}"
echo "Testing: $(which anomaly_detector)"

OUT="$(mktemp -dt anomaly_detector_test_XXXXXXXXXX)"
cd "${OUT}"
TESTLOG="${OUT}/anomaly-test-log"

cp "${SRC}/anomaly_detector_test_reporter.sh" .
touch messages

# Start the detector daemon.  With the --test option, the daemon reads input
# from ./messages, writes the anomaly reports into files in the current working
# directory, and invokes ./anomaly_detector_test_reporter.sh to report the
# anomalies.
anomaly_detector --test &
sleep 1

# Emit a kernel warning to messages and check that it is collected.
cat "${SRC}/TEST_WARNING" >> messages
sleep 1
check_log 1

# Add the same kernel warning to messages, verify that it is NOT collected
cat "${SRC}/TEST_WARNING" >> messages
sleep 1
check_log 1

# Add a slightly different kernel warning to messages, check that it is
# collected.
sed s/ttm_bo_vm.c/file_one.c/ < "${SRC}/TEST_WARNING" >> messages
sleep 1
check_log 2

# Emulate log rotation, add a kernel warning, and check.
mv messages messages.1
sed s/ttm_bo_vm.c/file_two.c/ < "${SRC}/TEST_WARNING" >> messages
sleep 2
check_log 3

# Emit a kernel warning in old format and check that it is collected.
cat "${SRC}/TEST_WARNING_OLD" >> messages
sleep 1
check_log 4

# Emit a service failure with restarts to messages. Check that it is collected
# only once, and that the correct exit status is reported.
cat "${SRC}/TEST_SERVICE_FAILURE" >> messages
sleep 1
check_log 5 '-exit2-'

# Emit a different service to messages. Check that it is collected once.
sed s/crash-crash/fresh-fresh/ < "${SRC}/TEST_SERVICE_FAILURE" >> messages
sleep 1
check_log 6

# Emit a kernel warning from a wifi driver.
rm -f "wifi-warning"
sed s/gpu\\/drm\\/ttm/net\\/wireless/ < "${SRC}/TEST_WARNING" >> messages
sleep 1
check_log 7
if [[ ! -f "wifi-warning" ]]; then
  fail "wifi-warning was not generated."
fi

# Emit a kernel warning from a suspend failure.
rm -f "suspend-warning"
sed s/gpu\\/drm\\/ttm/idle/ < "${SRC}/TEST_WARNING" >> messages
sleep 1
check_log 8
if [[ ! -f "suspend-warning" ]]; then
  fail "suspend-warning was not generated."
fi

# Emit a kernel warning in old arm64 format.  This was close enough to trigger
# collection but wasn't quite the format that we expected (we're fixing it in
# the kernel).  We'll use this as a test for the "unknown-function" fallback
# when we don't quite recognize the warning format.
cat "${SRC}/TEST_WARNING_OLD_ARM64" >> messages
sleep 1
check_log 9 '\-unknown\-function$'

rm -f "selinux-violation"
cat "${SRC}/TEST_SELINUX" >> messages
sleep 1
check_log 10 selinux

# Emit an ARC service failure to messages. Check that it is collected.
sed s/crash-crash/arc-crash/ < "${SRC}/TEST_SERVICE_FAILURE" >> messages
sleep 1
check_log 11 '-exit2-arc-'
if [[ ! -f "arc-service-failure" ]]; then
  fail "arc-service-failure was not generated."
fi

exit 0
