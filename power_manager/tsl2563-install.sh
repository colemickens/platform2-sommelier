#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Install tsl2563 module
echo tsl2563 0x29 > /sys/class/i2c-adapter/i2c-2/new_device
