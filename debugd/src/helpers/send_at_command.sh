#!/bin/sh

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Helper script for sending AT commands to a modem's serial interface
# and getting the result back.

set -e

command="$@"
if [ -z "${command}" ]; then
  echo "Usage: $0 <command>"
  exit 1
fi

tty=/dev/ttyUSB1
if [ ! -c "${tty}" ]; then
  echo "Missing modem tty device."
  exit 1
fi

/usr/sbin/chat -e ABORT '\nERROR' '' "${command}" '\nOK' < "${tty}" > "${tty}"

exit 0
