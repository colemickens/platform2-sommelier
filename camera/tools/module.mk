# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

tools_PC_DEPS = libcamera_timezone libchrome-$(BASE_VER) libbrillo-$(BASE_VER)
tools_CPPFLAGS := $(call get_pc_cflags,$(tools_PC_DEPS))
tools_LDLIBS := $(call get_pc_libs,$(tools_PC_DEPS))

CXX_BINARY(tools/generate_camera_profile): CPPFLAGS += $(tools_CPPFLAGS)
CXX_BINARY(tools/generate_camera_profile): LDLIBS += $(tools_LDLIBS)
CXX_BINARY(tools/generate_camera_profile): \
	$(tools_CXX_OBJECTS) \
	hal/usb/camera_characteristics.o \
	hal/usb/v4l2_camera_device.o

tools/generate_camera_profile: CXX_BINARY(tools/generate_camera_profile)

clean: CLEAN(tools/generate_camera_profile)

.PHONY: tools/generate_camera_profile
