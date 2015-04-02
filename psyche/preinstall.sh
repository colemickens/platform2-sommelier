#!/bin/bash
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

OUT=$1

sed \
  -e "s/@PV@/${PV}/g" \
  "lib/psyche/libpsyche.pc.in" >"${OUT}/libpsyche.pc"
