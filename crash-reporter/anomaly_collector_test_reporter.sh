#! /bin/sh
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Replacement for the crash reporter, for testing.  The anomaly report is passed
# via stdin.  Log the first line of the file, which by convention contains the
# anomaly hash.  If there is an argument, interpret it as a filename to touch.

set -e

# Send stdout & stderr to this log file.  This lets anomaly_collector_test.sh
# check the runtime behavior of the collector being tested.
exec 1>> anomaly-test-log
exec 2>> anomaly-test-log

head -1

if [ $# -eq 1 ]; then
  touch "$1"
fi
