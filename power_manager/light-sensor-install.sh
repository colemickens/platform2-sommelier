#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Install a light sensor driver on a given bus and address.
NAME=tsl2563
BUS=2
ADDRESS=0x29

# Use the new_device interface to instantiate it.
echo $NAME $ADDRESS > /sys/class/i2c-adapter/i2c-${BUS}/new_device
