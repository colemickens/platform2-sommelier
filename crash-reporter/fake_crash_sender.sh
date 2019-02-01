#!/bin/sh
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This shell script is used for unit testing.
# TODO(satorux): Remove this file once the crash sender is converted to C++.

set -e

# Emulate a failure, if requested.
if [ "$FAKE_CRASH_SENDER_SHOULD_FAIL" = "true" ]; then
  exit 1
fi

# Print the command line parameters to the output file.
echo "$@" >> "$FAKE_CRASH_SENDER_OUTPUT"

exit 0
