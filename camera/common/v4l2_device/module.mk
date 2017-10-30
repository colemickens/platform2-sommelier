# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

libcamera_v4l2_device_PC_DEPS := libchrome-$(BASE_VER)
libcamera_v4l2_device_CPPFLAGS := \
	$(call get_pc_cflags,$(libcamera_v4l2_device_PC_DEPS))
libcamera_v4l2_device_LDLIBS := $(call get_pc_libs,$(libcamera_v4l2_device_PC_DEPS))
CXX_STATIC_LIBRARY(common/v4l2_device/libcamera_v4l2_device.pic.a): \
	CPPFLAGS += $(libcamera_v4l2_device_CPPFLAGS)
CXX_STATIC_LIBRARY(common/v4l2_device/libcamera_v4l2_device.pic.a): \
	LDLIBS += $(libcamera_v4l2_device_LDLIBS)
CXX_STATIC_LIBRARY(common/v4l2_device/libcamera_v4l2_device.pic.a): $(common_v4l2_device_CXX_OBJECTS)
clean: CLEAN(common/v4l2_device/libcamera_v4l2_device.pic.a)
common/v4l2_device/libcamera_v4l2_device: CXX_STATIC_LIBRARY(common/v4l2_device/libcamera_v4l2_device.pic.a)
