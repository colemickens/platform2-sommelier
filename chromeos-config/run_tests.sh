#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to run all Python unit tests in cros_config.

# Run all unit tests (all files ending with test.py recursively)
while read -d $'\0' -r pytest; do
  echo "Running tests in ${pytest}"
  "./${pytest}" || exit 1
done < <(find -name '*test.py' -print0)
