#!/bin/bash
#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Very simple suite of manual tests for common.mk

set -e

log() {
  echo "$@"
  echo "$@" 1>&2
}

exec 1>test.log

log "TEST: out of dir"
mkdir -p foo
pushd foo
make -C ../ all
stat build-opt
make -C ../ tests
make -C ../ clean
(stat build-opt && false || true) &> /dev/null
popd
rmdir foo
log "PASSED"

log "TEST: out of dir, pwd=out"
mkdir -p foo
pushd foo
make -C ../ all OUT=$PWD
make -C ../ tests OUT=$PWD
make -C ../ clean OUT=$PWD
stat .dont_delete_on_clean
popd
rm foo/.dont_delete_on_clean
rmdir foo
log "PASSED"

log "TEST: out of dir, targets"
mkdir foo
pushd foo
make -C ../ 'CXX_BINARY(project_test)' \
            'CC_LIBRARY(component/subcomponent/libsubcomponent.so)'
stat build-opt
make -C ../ tests
make -C ../ clean
(stat build-opt && false || true) &> /dev/null
popd
rmdir foo
log "PASSED"

log "TEST: in dir"
make all
stat build-opt
make tests
make clean
(stat build-opt && false || true) &> /dev/null
log "PASSED"

log "TEST: in dir, qemu (no mounts)"
make all
stat build-opt
make tests USE_QEMU=1
make clean USE_QEMU=1
(stat build-opt && false || true) &> /dev/null
log "PASSED"

log "TEST: in dir, target"
make 'CXX_BINARY(project_test)'
stat build-opt
make tests
make clean
(stat build-opt && false || true) &> /dev/null
log "PASSED"

log "TEST: in dir, targets"
make 'CXX_BINARY(project_test)' \
     'CXX_LIBRARY(component/libcomponent.so)'
stat build-opt
make tests
make clean
(stat build-opt && false || true) &> /dev/null
log "PASSED"

log "TEST: non-existent automatic target"
# CXX_BINARY(project_test) exists, but not CC_BINARY. Make sure we don't allow
# incorrect targets on accident and let implicit rules ruin the day.
(make 'CC_BINARY(project_test)' || true) 2>&1
make clean
log "PASSED"

log ALL TESTS PASSED
