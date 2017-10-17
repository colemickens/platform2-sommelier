# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

libjda_PC_DEPS := libcamera_common libcamera_ipc libchrome-$(BASE_VER) \
	libmojo-$(BASE_VER)
libjda_CPPFLAGS := $(call get_pc_cflags,$(libjda_PC_DEPS))
libjda_LDLIBS := $(call get_pc_libs,$(libjda_PC_DEPS))
libjda_OBJS = common/jpeg/jpeg_decode_accelerator_impl.o
CXX_STATIC_LIBRARY(common/jpeg/libjda.pic.a): $(libjda_OBJS)
CXX_STATIC_LIBRARY(common/jpeg/libjda.pic.a): CPPFLAGS += $(libjda_CPPFLAGS)
CXX_STATIC_LIBRARY(common/jpeg/libjda.pic.a): LDLIBS += $(libjda_LDLIBS)
clean: CLEAN(common/jpeg/libjda.pic.a)
common/jpeg/libjda: CXX_STATIC_LIBRARY(common/jpeg/libjda.pic.a)

libjda_test_OBJS = common/jpeg/jpeg_decode_accelerator_test.o
libjda_test_PC_DEPS := libjda libcamera_common libcamera_ipc \
	libchrome-$(BASE_VER) libmojo-$(BASE_VER) libyuv
libjda_test_CPPFLAGS := $(call get_pc_cflags,$(libjda_test_PC_DEPS))
libjda_test_LDLIBS := $(call get_pc_libs,$(libjda_test_PC_DEPS)) -lgtest -ljpeg
CXX_BINARY(common/jpeg/libjda_test): $(libjda_test_OBJS)
CXX_BINARY(common/jpeg/libjda_test): CPPFLAGS += $(libjda_test_CPPFLAGS)
CXX_BINARY(common/jpeg/libjda_test): LDLIBS += $(libjda_test_LDLIBS)
clean: CLEAN(common/jpeg/libjda_test)
common/jpeg/libjda_test: CXX_BINARY(common/jpeg/libjda_test)
