# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

PC_DEPS := arc-camera3-android-headers
PC_CFLAGS := $(call get_pc_cflags,$(PC_DEPS))

CPPFLAGS += -D_GNU_SOURCE -I$(abspath $(SRC)) -I$(abspath $(SRC)/include) \
	    $(PC_CFLAGS)
CXXFLAGS += -std=c++11 -Wpointer-arith
CFLAGS += -std=c99

libcamera_client: libcamera_client/all

libcamera_metadata: libcamera_metadata/all

.PHONY: libcamera_client libcamera_metadata
