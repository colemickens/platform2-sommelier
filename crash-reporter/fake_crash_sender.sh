#!/bin/sh
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This shell script is used for unit testing.
# TODO(satorux): Remove this file once the crash sender is converted to C++.

set -e

# Print the command line parameters to the output file.
echo "$1 $2" >> "$FAKE_CRASH_SENDER_OUTPUT"

exit 0
