#!/bin/bash
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DIR=$(cd -P -- "$(dirname -- "$0")" && pwd -P)
ROOT_DIR=$(cd -P -- "$(dirname -- "$0")/../.." && pwd -P)

cd $ROOT_DIR

gyp -Ilibweave_common.gypi --toplevel-dir=. -f ninja $DIR/weave.gyp

if [ -z "$BUILD_CONFIG" ]; then
   export BUILD_CONFIG=Debug
fi

export BUILD_TARGET=$*
if [ -z "$BUILD_TARGET" ]; then
   export BUILD_TARGET="weave libweave_testrunner"
fi

export CORES=`cat /proc/cpuinfo | grep processor | wc -l`
ninja -j $CORES -C out/${BUILD_CONFIG} $BUILD_TARGET || exit 1

if [[ $BUILD_TARGET == *"libweave_testrunner"* ]]; then
  out/${BUILD_CONFIG}/libweave_testrunner
fi
