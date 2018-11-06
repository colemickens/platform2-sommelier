#! /bin/bash

# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# Copy the json test config locally
cp libcros_config/test.json test.json
cp libcros_config/test_arm.json test_arm.json
