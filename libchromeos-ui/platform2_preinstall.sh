#!/bin/bash

# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

OUT=$1
shift
for v; do
  sed -e "s/@BSLOT@/${v}/g" \
    libchromeos-ui.pc.in > "${OUT}/lib/libchromeos-ui-${v}.pc"
done
