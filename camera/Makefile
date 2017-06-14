# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

PC_DEPS := libchrome-$(BASE_VER)
PC_CFLAGS := $(call get_pc_cflags,$(PC_DEPS))
PC_LIBS := $(call get_pc_libs,$(PC_DEPS))

ANDROID_INCLUDE := $(abspath $(SRC)/android/header_files/include)
ANDROID_INCLUDE_PATHS := \
	-I$(ANDROID_INCLUDE) \
	-I$(ANDROID_INCLUDE)/hardware/libhardware/include \
	-I$(ANDROID_INCLUDE)/system/core/include

LDLIBS += $(PC_LIBS)

CPPFLAGS += $(PC_CFLAGS) -D_GNU_SOURCE -I$(abspath $(SRC)) \
	    -I$(abspath $(SRC)/include) $(ANDROID_INCLUDE_PATHS)
CXXFLAGS += -std=c++11 -Wpointer-arith
CFLAGS += -std=c99

hal_adapter:
	$(MAKE) hal_adapter/mojo/mojo_templates
	$(MAKE) hal_adapter/arc_camera3_service

.PHONY: hal_adapter

libcbm: common/libcbm

.PHONY: libcbm

camera_hal_usb: hal/usb/camera_hal

.PHONY: camera_hal_usb

camera3_test: camera3_test/arc_camera3_test

.PHONY: camera3_test
