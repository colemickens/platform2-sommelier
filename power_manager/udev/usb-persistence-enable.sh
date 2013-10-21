#!/bin/sh
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enable USB persistence during system suspend for the selected device.

# 99-usb-persistence-enable.rules passes the sysfs device path as $1.
echo 1 > "$1/power/persist"
