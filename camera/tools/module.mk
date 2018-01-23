# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

tools_PC_DEPS = libchrome-$(BASE_VER) libbrillo-$(BASE_VER)
tools_CPPFLAGS := $(call get_pc_cflags,$(tools_PC_DEPS))
tools_LDLIBS := $(call get_pc_libs,$(tools_PC_DEPS))

timezone_CPPFLAGS := $(call get_pc_cflags,libcamera_timezone)
timezone_LDLIBS := $(call get_pc_libs,libcamera_timezone)

CXX_BINARY(tools/generate_camera_profile): CPPFLAGS += \
	$(timezone_CPPFLAGS) \
	$(tools_CPPFLAGS)
CXX_BINARY(tools/generate_camera_profile): LDLIBS += \
	$(timezone_LDLIBS) \
	$(tools_LDLIBS)
CXX_BINARY(tools/generate_camera_profile): \
	tools/generate_camera_profile.o \
	hal/usb/camera_characteristics.o \
	hal/usb/v4l2_camera_device.o

tools/generate_camera_profile: CXX_BINARY(tools/generate_camera_profile)

clean: CLEAN(tools/generate_camera_profile)

.PHONY: tools/generate_camera_profile

CXX_BINARY(tools/cros-camera-tool): CPPFLAGS += $(tools_CPPFLAGS)
CXX_BINARY(tools/cros-camera-tool): LDLIBS += $(tools_LDLIBS)
CXX_BINARY(tools/cros-camera-tool): \
	tools/cros_camera_tool.o

tools/cros-camera-tool: CXX_BINARY(tools/cros-camera-tool)

clean: CLEAN(tools/cros-camera-tool)

.PHONY: tools/cros-camera-tool
