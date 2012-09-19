#!/bin/sh
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enable wakeup for usb-hid devices

listed_by_type() {
    device=$1
    device_base=`basename $device`
    list=$2
    for driver_type in $list; do
        if grep -q DRIVER=$driver_type $device/uevent; then
            return 0
        fi
        # Check child devices as well.  The actual driver type is
        # listed in a child device, not the top-level device.
        for subdevice in $device/$device_base*; do
            if [ -f $subdevice/uevent ]; then
                if grep -q DRIVER=$driver_type\
                  $subdevice/uevent; then
                    return 0
                fi
            fi
        done
    done
    return 1
}

device=/sys/bus/usb/devices/$1
if listed_by_type $device usbhid; then
    echo "enabled" > $device/power/wakeup
fi
