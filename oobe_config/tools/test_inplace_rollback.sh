#!/bin/bash

# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a helper script for testing an in place rollback.
# It doesn't change the image, it initiates all the state
# saving, then triggers a powerwash and reboot. When the device
# reboots it will be in the state as if it was rolled back to
# the current version.
#
# This script isn't intended to be shipped on the device, it's
# only for manual testing.

echo "Staging rollback files..."
if ! rollback_prepare_save; then
  echo "rollback_prepare_save failed"
  exit 1
fi

echo "Encrypting and saving rollback files..."
if ! sudo -u oobe_config_save oobe_config_save; then
  echo "rollback_prepare_save failed"
  exit 1
fi

echo "Rebooting with powerwash in 10 seconds..."
sleep 10
# Don't write the reset file until after the timeout
echo "fast safe keepimg rollback" > \
  /mnt/stateful_partition/factory_install_reset
echo "Rebooting now..."
reboot
