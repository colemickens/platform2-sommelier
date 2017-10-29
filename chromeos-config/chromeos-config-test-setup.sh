#! /bin/bash

# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

./cros_config_host.py write-target-dirs >libcros_config/target_dirs.dtsi

# Compile the test .dts files.
dtc -I dts -O dtb -o test.dtb libcros_config/test.dts
dtc -I dts -O dtb -o test_bad_struct.dtb libcros_config/test_bad_struct.dts
