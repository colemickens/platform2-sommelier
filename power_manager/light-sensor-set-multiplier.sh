#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Set light sensor calibration value.  The default is 1000, but it may be
# overwritten by ebuild
TUNEVAL=1000
echo $TUNEVAL > /sys/class/iio/device0/calib0
