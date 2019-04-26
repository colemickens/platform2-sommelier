#!/bin/sh
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

sysfs_config_file=/sys/devices/virtual/misc/hci_le/le_splitter_enabled
newblue_config_file=/var/lib/bluetooth/newblue

# Set the group of sysfs config file to "bluetooth" to allow bluetoothd to read
# the status of the splitter.
chown :bluetooth "${sysfs_config_file}"

# Make sure the bluetooth module is loaded, otherwise the sysfs splitter config
# file won't exist.
modprobe bluetooth

if [ ! -e "${sysfs_config_file}" ]; then
  # The config file doesn't exist, this means that this device doesn't support
  # LE splitter, so just ignore and finish the task.
  exit 0
fi

if [ -e "${newblue_config_file}" ]; then
  cat "${newblue_config_file}" >"${sysfs_config_file}"
else
  # Default for LE splitter is disabled.
  echo 0 >"${sysfs_config_file}"
fi

# Reinstall Marvell SDIO Bluetooth driver to trigger re-initialization of the
# Bluetooth device.
rmmod btmrvl_sdio
modprobe btmrvl_sdio
# Simulate that the USB Bluetooth adapter is re-plugged to trigger BlueZ to
# re-initialize it.
# For devices with UART Bluetooth adapter we don't need to trigger the
# re-initialization here since we can defer the brcm_patchram_plus daemon to run
# later after this script is done.
rmmod btusb
modprobe btusb
