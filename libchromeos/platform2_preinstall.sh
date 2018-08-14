#!/bin/bash

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

OUT=$1
shift
for v; do
  # Extract all the libchromeos sublibs from 'dependencies' section of
  # 'libchromeos-<(libbase_ver)' target in libchromeos.gypi and convert them
  # into an array of "-lchromeos-<sublib>-<v>" flags.
  sublibs=($(sed -n "
     /'target_name': 'libchromeos-<(libbase_ver)'/,/target_name/ {
       /dependencies/,/],/ {
         /libchromeos/ {
           s:[',]::g
           s:<(libbase_ver):${v}:g
           s:libchromeos:-lchromeos:
           p
         }
       }
     }" libchromeos.gypi))

  echo "GROUP ( AS_NEEDED ( ${sublibs[@]} ) )" > "${OUT}"/lib/libchromeos-${v}.so

  deps=$(<"${OUT}"/gen/libchromeos-${v}-deps.txt)
  pc="${OUT}"/lib/libchromeos-${v}.pc

  sed \
    -e "s/@BSLOT@/${v}/g" \
    -e "s/@PRIVATE_PC@/${deps}/g" \
    "libchromeos.pc.in" > "${pc}"

  deps_test=$(<"${OUT}"/gen/libchromeos-test-${v}-deps.txt)
  deps_test+=" libchromeos-${v}"
  sed \
    -e "s/@BSLOT@/${v}/g" \
    -e "s/@PRIVATE_PC@/${deps_test}/g" \
    "libchromeos-test.pc.in" > "${OUT}/lib/libchromeos-test-${v}.pc"


  deps_glib=$(<"${OUT}"/gen/libchromeos-glib-${v}-deps.txt)
  pc_glib="${OUT}"/lib/libchromeos-glib-${v}.pc

  sed \
    -e "s/@BSLOT@/${v}/g" \
    -e "s/@PRIVATE_PC@/${deps_glib}/g" \
    "libchromeos-glib.pc.in" > "${pc_glib}"
done
