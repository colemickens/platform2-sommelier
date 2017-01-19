# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

usb_PC_DEPS = libexif libyuv
usb_CPPFLAGS := $(call get_pc_cflags,$(usb_PC_DEPS))
usb_LDLIBS := $(call get_pc_libs,$(usb_PC_DEPS)) -ljpeg

CXX_LIBRARY(usb/camera_hal.so): CPPFLAGS += $(usb_CPPFLAGS)
CXX_LIBRARY(usb/camera_hal.so): LDLIBS += $(usb_LDLIBS)
CXX_LIBRARY(usb/camera_hal.so): \
	$(ANDROID_OBJECTS) \
	$(COMMON_OBJECTS) \
	$(usb_CXX_OBJECTS)

usb/camera_hal: CXX_LIBRARY(usb/camera_hal.so)

clean: CLEAN(usb/camera_hal.so)

.PHONY: usb/camera_hal
