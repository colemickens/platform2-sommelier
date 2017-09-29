#! /bin/bash

# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# Compile the test .dts files.
dtc -I dts -O dtb -o test.dtb libcros_config/test.dts
dtc -I dts -O dtb -o test_bad_struct.dtb libcros_config/test_bad_struct.dts

# Copy the json test config locally
cp libcros_config/test_config.json test_config.json
