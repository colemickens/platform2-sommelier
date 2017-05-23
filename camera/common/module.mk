# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

future_unittest_OBJS = common/future.o common/future_unittest.o
future_unittest_LIBS = -lgtest
CXX_BINARY(common/future_unittest): $(future_unittest_OBJS)
CXX_BINARY(common/future_unittest): LDLIBS += $(future_unittest_LIBS)
clean: CLEAN(common/future_unittest)
tests: CXX_BINARY(common/future_unittest)

camera_buffer_mapper_unittest_OBJS = \
	common/camera_buffer_mapper.o \
	common/camera_buffer_mapper_unittest.o
camera_buffer_mapper_unittest_PC_DEPS := gbm libdrm
camera_buffer_mapper_unittest_CPPFLAGS = \
	$(call get_pc_cflags,$(camera_buffer_mapper_unittest_PC_DEPS))
camera_buffer_mapper_unittest_LIBS = \
	$(call get_pc_libs,$(camera_buffer_mapper_unittest_PC_DEPS)) \
	-lgmock -lgtest -lpthread
CXX_BINARY(common/camera_buffer_mapper_unittest): \
	$(camera_buffer_mapper_unittest_OBJS)
CXX_BINARY(common/camera_buffer_mapper_unittest): \
	CPPFLAGS += $(camera_buffer_mapper_unittest_CPPFLAGS)
CXX_BINARY(common/camera_buffer_mapper_unittest): \
	LDLIBS += $(camera_buffer_mapper_unittest_LIBS)
clean: CLEAN(common/camera_buffer_mapper_unittest)
tests: CXX_BINARY(common/camera_buffer_mapper_unittest)

libcbm_PC_DEPS := gbm libchrome-$(BASE_VER) libdrm
libcbm_CPPFLAGS := $(call get_pc_cflags,$(libcbm_PC_DEPS))
libcbm_LDLIBS := $(call get_pc_libs,$(libcbm_PC_DEPS))
libcbm_OBJS = \
	common/camera_buffer_mapper.o \
	common/camera_buffer_mapper_internal.o
CXX_LIBRARY(common/libcbm.so): $(libcbm_OBJS)
CXX_LIBRARY(common/libcbm.so): CPPFLAGS += $(libcbm_CPPFLAGS)
CXX_LIBRARY(common/libcbm.so): LDLIBS += $(libcbm_LDLIBS)
clean: CLEAN(common/libcbm.so)
common/libcbm: CXX_LIBRARY(common/libcbm.so)

# To link against object files under common/, add $(COMMON_OBJECTS) to the
# dependency list of your target.
COMMON_OBJECTS := \
	common/future.o

COMMON_OBJECTS_FOR_USB_HAL := \
	common/exif_utils.o \
	common/future.o \
	common/jpeg_compressor.o
