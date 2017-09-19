#!/bin/sh
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to run basic tests on the config example in README.md

# Use awk to extract the example (between ``` markers) and add a header / footer
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
