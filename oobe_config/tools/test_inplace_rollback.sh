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

rollback_prepare_save
sudo -u oobe_config_save oobe_config_save
echo "fast safe keepimg rollback" > \
  /mnt/stateful_partition/factory_install_reset

echo "Rebooting with powerwash in 5 seconds..."
sleep 5
echo "Rebooting now..."
reboot
