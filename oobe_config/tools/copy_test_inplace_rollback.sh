#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a helper script for copying and running the test_inplace_rollback
# script on a DUT. The target DUT's ip must be supplied as an argument.
#
# First the script is copied, then the permissions are updated, and finally
# test_inplace_rollback.sh is run.
#
# This script isn't intended to be shipped on the device, it's
# only for manual testing.

DEST_DIR=/tmp
TEST_SCRIPT=test_inplace_rollback.sh
TOOLS_DIR=$(dirname "$BASH_SOURCE")
USER=root

if [ -z "$1" ]
  then
    echo "No DUT specified."
    exit 1
fi

echo "Copying ${TEST_SCRIPT} to $1"
if ! scp $TOOLS_DIR/$TEST_SCRIPT $USER@$1:$DEST_DIR/; then
  echo "scp failed"
  exit 1
fi

echo "Updating permissions of ${TEST_SCRIPT} on $1"
if ! ssh $USER@$1 chmod 700 $DEST_DIR/$TEST_SCRIPT; then
  echo "chmod failed"
  exit 1
fi

echo "Running ${TEST_SCRIPT}"
if ! ssh $USER@$1 $DEST_DIR/$TEST_SCRIPT; then
  echo "running ${TEST_SCRIPT} failed"
  exit 1
fi
