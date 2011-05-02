#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Set light sensor calibration value.  The default is 1000, but it may be
# overwritten by ebuild
TUNEVAL=1000
if [ -w /sys/class/iio/device0/calib0 ]; then
	echo $TUNEVAL > /sys/class/iio/device0/calib0
fi
if [ -w /sys/bus/iio/devices/device0/illuminance0_calibbias ]; then
	echo $TUNEVAL > /sys/bus/iio/devices/device0/illuminance0_calibbias
fi
if [ -w /sys/bus/iio/devices/device0/intensity0_both_calibgain ]; then
	echo $TUNEVAL > /sys/bus/iio/devices/device0/intensity0_both_calibgain
fi
