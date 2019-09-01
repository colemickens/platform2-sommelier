#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to run all Python unit tests in cros_config.

python2 -m unittest discover -p '*test.py'
