# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

image_processor_unittest_OBJS = \
	hal/usb/frame_buffer.o \
	hal/usb/image_processor.o \
	hal/usb/unittest/image_processor_unittest.o
image_processor_unittest_PC_DEPS = libcamera_client libcamera_exif \
	libcamera_jpeg libcamera_metadata libcbm libchrome-$(BASE_VER) libsync libyuv
image_processor_unittest_CPPFLAGS := \
	$(call get_pc_cflags,$(image_processor_unittest_PC_DEPS))
image_processor_unittest_LIBS := \
	$(call get_pc_libs,$(image_processor_unittest_PC_DEPS)) -lgtest
CXX_BINARY(hal/usb/unittest/image_processor_unittest): \
	$(image_processor_unittest_OBJS)
CXX_BINARY(hal/usb/unittest/image_processor_unittest): \
	CPPFLAGS += $(image_processor_unittest_CPPFLAGS)
CXX_BINARY(hal/usb/unittest/image_processor_unittest): \
	LDLIBS += $(image_processor_unittest_LIBS)

hal_usb_test: CXX_BINARY(hal/usb/unittest/image_processor_unittest)
