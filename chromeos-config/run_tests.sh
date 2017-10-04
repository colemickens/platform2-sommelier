#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to run all unit tests in cros_config as well as a basic tests on the
# config example in README.md

# Run all unit tests (all files ending with test.py recursively)
while read -d $'\0' -r pytest; do
  echo "Running tests in ${pytest}"
  "./${pytest}" || exit 1
done < <(find -name '*test.py' -print0)

# Test the README file, use awk to extract the example (between ``` markers) and
# add a header / footer
tmp="/tmp/test.$$"
awk 'BEGIN {print "/dts-v1/; / {" } END {print "};"} \
            /```/{flag = !flag; next} flag' README.md > "${tmp}"
if dtc "${tmp}"  >/dev/null; then
  rm "${tmp}"
  echo "All OK"
else
  echo "See source file in ${tmp}"
  exit 1
fi
