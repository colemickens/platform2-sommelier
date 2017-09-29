#! /bin/sh
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Replacement for the crash reporter, for testing.  The file containing the
# anomaly report should be passed in as an argument. Log the first line of the
# file, which by convention contains the anomaly hash, and remove the file. If
# there is a second argument, interpret it as a filename to touch.

set -e

exec 1>> anomaly-test-log
exec 2>> anomaly-test-log

head -1 "$1"
rm "$1"

if [ $# -eq 2 ]; then
  touch "$2"
fi
