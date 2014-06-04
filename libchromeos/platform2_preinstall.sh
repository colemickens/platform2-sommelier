#!/bin/bash

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

OUT=$1
shift
for v; do
  sublibs=( 'core' 'cryptohome' 'ui' )
  sublibs=("${sublibs[@]/#/-lchromeos-}")
  sublibs="${sublibs[@]/%/-${v}}"
  echo "GROUP ( AS_NEEDED ( ${sublibs} ) )" > "${OUT}"/lib/libchromeos-${v}.so

  deps=$(<"${OUT}"/gen/libchromeos-${v}-deps.txt)
  pc="${OUT}"/lib/libchromeos-${v}.pc

  sed \
    -e "s/@BSLOT@/${v}/g" \
    -e "s/@PRIVATE_PC@/${deps}/g" \
    "libchromeos.pc.in" > "${pc}"
done
