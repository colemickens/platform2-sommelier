#!/bin/bash
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DIR=$(cd -P -- "$(dirname -- "$0")" && pwd -P)
ROOT_DIR=$(cd -P -- "$(dirname -- "$0")/../.." && pwd -P)

sudo apt-get install \
  autoconf \
  automake \
  binutils \
  gyp \
  hostapd \
  libavahi-client-dev \
  libcurl4-openssl-dev \
  libexpat1-dev \
  libtool \
  ninja-build \

mkdir $ROOT_DIR/third_party/lib $ROOT_DIR/third_party/include 2> /dev/null

# Make gtest and gmock
cd $ROOT_DIR/third_party
rm -rf googletest

git clone https://github.com/google/googletest.git || exit 1
cd $ROOT_DIR/third_party/googletest

# gtest is in process of changing of dir structure and it has broken build
# files. So this is temporarily workaround to fix that.
git reset --hard d945d8c000a0ade73585d143532266968339bbb3
mv googletest googlemock/gtest

for SUB_DIR in googlemock/gtest googlemock; do
  cd $ROOT_DIR/third_party/googletest/$SUB_DIR || exit 1
  autoreconf -fvi || exit 1
  ./configure --disable-shared || exit 1
  make || exit 1
  cp -rf include/* $ROOT_DIR/third_party/include/ || exit 1
  cp -rf lib/.libs/* $ROOT_DIR/third_party/lib/ || exit 1
done
rm -rf $ROOT_DIR/third_party/googletest

# Make libevent.
# Example uses libevent to implement HTTPS server. This capability is
# available only in version 2.1.x-alpha. Step could be replaced with apt-get
# in future.
cd $ROOT_DIR/third_party
rm -rf libevent

git clone https://github.com/libevent/libevent.git || exit 1
cd libevent || exit 1
./autogen.sh || exit 1
./configure --disable-shared || exit 1
make || exit 1
make verify || exit 1
cp -rf include/* $ROOT_DIR/third_party/include/ || exit 1
cp -rf .libs/lib* $ROOT_DIR/third_party/lib/ || exit 1
rm -rf $ROOT_DIR/third_party/libevent
