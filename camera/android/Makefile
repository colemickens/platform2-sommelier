# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

ANDROID_INCLUDE := $(abspath $(SRC)/include)
ANDROID_INCLUDE_PATHS := \
	-I$(ANDROID_INCLUDE) \
	-I$(ANDROID_INCLUDE)/hardware/libhardware/include \
	-I$(ANDROID_INCLUDE)/system/core/include

CPPFLAGS += -D_GNU_SOURCE -I$(abspath $(SRC)) -I$(abspath $(SRC)/include) \
	    $(ANDROID_INCLUDE_PATHS)
CXXFLAGS += -std=c++11 -Wpointer-arith
CFLAGS += -std=c99

libcamera_client: libcamera_client/all

libcamera_metadata: libcamera_metadata/all

libsync: libsync/all

.PHONY: libcamera_client libcamera_metadata libsync
