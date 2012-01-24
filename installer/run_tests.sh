#!/bin/bash
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
make -C ../ tests
stat cros_installer_test
make -C ../ clean
((stat cros_installer_test && false) || true) 2>&1
stat .dont_delete_on_clean
popd
rm foo/.dont_delete_on_clean
rmdir foo
log "PASSED"

log "TEST: out of dir, out=pwd"
mkdir -p foo
pushd foo
make -C ../ all OUT=$PWD
make -C ../ tests OUT=$PWD
stat cros_installer_test
make -C ../ clean OUT=$PWD
((stat cros_installer_test && false) || true) 2>&1
stat .dont_delete_on_clean
popd
rm foo/.dont_delete_on_clean
rmdir foo
log "PASSED"

log "TEST: out of dir, out=src/build-opt"
mkdir -p foo
pushd foo
make -C ../ all OUT=$PWD/../build-opt
ls ../build-opt
make -C ../ tests OUT=$PWD/../build-opt
make -C ../ clean OUT=$PWD/../build-opt
((stat ../build-opt/cros_installer_test && false) || true) 2>&1
((stat ../build-opt && false) || true) 2>&1
popd
rmdir foo
log "PASSED"


log "TEST: out of dir, targets"
mkdir foo
pushd foo
make -C ../ 'CXX_BINARY(cros_installer_test)' \
            'CC_LIBRARY(component/subcomponent/libsubcomponent.so)'
stat cros_installer_test
stat component/subcomponent/libsubcomponent.so
make -C ../ tests
make -C ../ clean
((stat cros_installer_test && false) || true) 2>&1
stat .dont_delete_on_clean
popd
rm foo/.dont_delete_on_clean
rmdir foo
log "PASSED"

log "TEST: in dir"
make all
make tests
stat cros_installer_test
make clean
((stat cros_installer_test && false) || true) 2>&1
log "PASSED"

log "TEST: in dir, qemu (no mounts)"
make all
make tests USE_QEMU=1
stat cros_installer_test
make clean USE_QEMU=1
((stat cros_installer_test && false) || true) 2>&1
log "PASSED"

log "TEST: in dir, target"
make 'CXX_BINARY(cros_installer_test)'
stat cros_installer_test
make tests
make clean
((stat cros_installer_test && false) || true) 2>&1
log "PASSED"

log "TEST: in dir, targets"
make 'CXX_BINARY(cros_installer_test)' \
     'CXX_LIBRARY(component/libcomponent.so)'
stat cros_installer_test
stat component/libcomponent.so
make tests
make clean
((stat cros_installer_test && false) || true) 2>&1
((stat component/libcomponent.so && false) || true) 2>&1
log "PASSED"

log "TEST: non-existent automatic target"
# CXX_BINARY(cros_installer_test) exists, but not CC_BINARY.
# Make sure we don't allow
# incorrect targets on accident and let implicit rules ruin the day.
(make 'CC_BINARY(cros_installer_test)' || true) 2>&1
make clean
log "PASSED"

log ALL TESTS PASSED
