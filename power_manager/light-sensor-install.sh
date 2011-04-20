#!/bin/sh

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Here's the 'udevadm monitor --property' we're responding to...
# ...at least on
#KERNEL[1302714506.346339] add      /bus/pci/drivers/i801_smbus (drivers)
#UDEV_LOG=6
#ACTION=add
#DEVPATH=/bus/pci/drivers/i801_smbus
#SUBSYSTEM=drivers

# We're always going to install a tsl2563 light sensor for now.  It should
# always be on an i2c bus and have address 0x29.
NAME=tsl2563
ADDRESS=0x29

# Be generic--don't assume we end up on any given numbered i2c device.
# On 2.6.32 on Cr-48, we get:
#   /sys/bus/pci/drivers/i801_smbus/0000:00:1f.3/i2c-2
# On 2.6.38 on Cr-48, we get:
#   /sys/bus/pci/drivers/i801_smbus/0000:00:1f.3/i2c-14
#
# TODO(dianders): A better way to find the i2c bus?
I2C_PATH=$(echo /sys/$DEVPATH/*/i2c-*)

# Use the new_device interface to instantiate it.
echo $NAME $ADDRESS > "$I2C_PATH"/new_device
