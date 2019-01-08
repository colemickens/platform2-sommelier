#!/bin/sh

# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# TODO(fqj): Delete once R74 branches.
VERBOSE=""
if [ -f /home/chronos/.chaps_debug_2 ]; then
  VERBOSE="--v=2"
  rm /home/chronos/.chaps_debug_2
elif [ -f /home/chronos/.chaps_debug_1 ]; then
  VERBOSE="--v=1"
  rm /home/chronos/.chaps_debug_1
fi

exec chapsd ${VERBOSE} --auto_load_system_token
