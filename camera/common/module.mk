# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

future_unittest_OBJS = common/future.o common/future_unittest.o
future_unittest_PC_DEPS := libchrome-$(BASE_VER)
future_unittest_CPPFLAGS := $(call get_pc_cflags,$(future_unittest_PC_DEPS))
future_unittest_LIBS = $(call get_pc_libs,$(future_unittest_PC_DEPS)) -lgtest
CXX_BINARY(common/future_unittest): $(future_unittest_OBJS)
CXX_BINARY(common/future_unittest): CPPFLAGS += $(future_unittest_CPPFLAGS)
CXX_BINARY(common/future_unittest): LDLIBS += $(future_unittest_LIBS)
clean: CLEAN(common/future_unittest)
tests: CXX_BINARY(common/future_unittest)

camera_buffer_manager_unittest_OBJS = \
	common/camera_buffer_manager_impl.o \
	common/camera_buffer_manager_impl_unittest.o
camera_buffer_manager_unittest_PC_DEPS := gbm libchrome-$(BASE_VER) libdrm
camera_buffer_manager_unittest_CPPFLAGS = \
	$(call get_pc_cflags,$(camera_buffer_manager_unittest_PC_DEPS))
camera_buffer_manager_unittest_LIBS = \
	$(call get_pc_libs,$(camera_buffer_manager_unittest_PC_DEPS)) \
	-lgmock -lgtest -lpthread
CXX_BINARY(common/camera_buffer_manager_unittest): \
	$(camera_buffer_manager_unittest_OBJS)
CXX_BINARY(common/camera_buffer_manager_unittest): \
	CPPFLAGS += $(camera_buffer_manager_unittest_CPPFLAGS)
CXX_BINARY(common/camera_buffer_manager_unittest): \
	LDLIBS += $(camera_buffer_manager_unittest_LIBS)
clean: CLEAN(common/camera_buffer_manager_unittest)
tests: CXX_BINARY(common/camera_buffer_manager_unittest)

libcbm_PC_DEPS := gbm libchrome-$(BASE_VER) libdrm
libcbm_CPPFLAGS := $(call get_pc_cflags,$(libcbm_PC_DEPS))
libcbm_LDLIBS := $(call get_pc_libs,$(libcbm_PC_DEPS))
libcbm_OBJS = \
	common/camera_buffer_manager_impl.o \
	common/camera_buffer_manager_internal.o
CXX_LIBRARY(common/libcbm.so): $(libcbm_OBJS)
CXX_LIBRARY(common/libcbm.so): CPPFLAGS += $(libcbm_CPPFLAGS)
CXX_LIBRARY(common/libcbm.so): LDLIBS += $(libcbm_LDLIBS)
clean: CLEAN(common/libcbm.so)
common/libcbm: CXX_LIBRARY(common/libcbm.so)

arc_camera_algo_PC_DEPS := libchrome-$(BASE_VER) libmojo-$(BASE_VER)
arc_camera_algo_CPPFLAGS := $(call get_pc_cflags,$(arc_camera_algo_PC_DEPS))
arc_camera_algo_LDLIBS := $(call get_pc_libs,$(arc_camera_algo_PC_DEPS)) -ldl
arc_camera_algo_OBJS = \
	common/camera_algorithm_adapter.o \
	common/camera_algorithm_main.o \
	common/camera_algorithm_ops_impl.o \
	common/future.o \
	common/mojo/camera_algorithm.mojom.o \
	hal_adapter/ipc_util.o
CXX_BINARY(common/arc_camera_algo): $(arc_camera_algo_OBJS)
CXX_BINARY(common/arc_camera_algo): CPPFLAGS += $(arc_camera_algo_CPPFLAGS)
CXX_BINARY(common/arc_camera_algo): LDLIBS += $(arc_camera_algo_LDLIBS)
libcab_PC_DEPS := libchrome-$(BASE_VER) libmojo-$(BASE_VER)
libcab_CPPFLAGS := $(call get_pc_cflags,$(libcab_PC_DEPS))
libcab_LDLIBS := $(call get_pc_libs,$(libcab_PC_DEPS))
libcab_OBJS = \
	common/camera_algorithm_bridge_impl.o \
	common/camera_algorithm_callback_ops_impl.o \
	common/future.o \
	common/mojo/camera_algorithm.mojom.o
CXX_STATIC_LIBRARY(common/libcab.pic.a): $(libcab_OBJS)
CXX_STATIC_LIBRARY(common/libcab.pic.a): CPPFLAGS += $(libcab_CPPFLAGS)
CXX_STATIC_LIBRARY(common/libcab.pic.a): LDLIBS += $(libcab_LDLIBS)
clean: CLEAN(common/arc_camera_algo) CLEAN(common/libcab.pic.a)
common/libcab: \
	CXX_BINARY(common/arc_camera_algo) \
	CXX_STATIC_LIBRARY(common/libcab.pic.a)

fake_libcam_algo_PC_DEPS := libchrome-$(BASE_VER) libmojo-$(BASE_VER)
fake_libcam_algo_CPPFLAGS := $(call get_pc_cflags,$(fake_libcam_algo_PC_DEPS))
fake_libcam_algo_LDLIBS := $(call get_pc_libs,$(fake_libcam_algo_PC_DEPS))
fake_libcam_algo_OBJS = common/fake_libcam_algo.o
CXX_LIBRARY(common/libcam_algo.so): $(fake_libcam_algo_OBJS)
CXX_LIBRARY(common/libcam_algo.so): CPPFLAGS += $(fake_libcam_algo_CPPFLAGS)
CXX_LIBRARY(common/libcam_algo.so): LDLIBS += $(fake_libcam_algo_LDLIBS)
libcab_test_PC_DEPS := libcab libchrome-$(BASE_VER) libmojo-$(BASE_VER)
libcab_test_CPPFLAGS := $(call get_pc_cflags,$(libcab_test_PC_DEPS))
libcab_test_LDLIBS := $(call get_pc_libs,$(libcab_test_PC_DEPS)) -Wl,-Bstatic \
	-lgtest -Wl,-Bdynamic -lrt -pthread
libcab_test_OBJS = common/libcab_test_main.o
CXX_BINARY(common/libcab_test): CPPFLAGS += $(libcab_test_CPPFLAGS)
CXX_BINARY(common/libcab_test): LDLIBS += $(libcab_test_LDLIBS)
CXX_BINARY(common/libcab_test): $(libcab_test_OBJS)
clean: CLEAN(common/libcab_test) CLEAN(common/libcam_algo.so)
common/libcab_test: \
	CXX_BINARY(common/libcab_test) \
	CXX_LIBRARY(common/libcam_algo.so)

libcamera_jpeg_OBJS = common/jpeg_compressor.o
libcamera_jpeg_PC_DEPS := libchrome-$(BASE_VER)
libcamera_jpeg_CPPFLAGS := $(call get_pc_cflags,$(libcamera_jpeg_PC_DEPS))
libcamera_jpeg_LDLIBS := $(call get_pc_libs,$(libcamera_jpeg_PC_DEPS))
CXX_STATIC_LIBRARY(common/libcamera_jpeg.pic.a): \
	CPPFLAGS += $(libcamera_jpeg_CPPFLAGS)
CXX_STATIC_LIBRARY(common/libcamera_jpeg.pic.a): \
	LDLIBS += $(libcamera_jpeg_LDLIBS) -ljpeg
CXX_STATIC_LIBRARY(common/libcamera_jpeg.pic.a): $(libcamera_jpeg_OBJS)
clean: CLEAN(common/libcamera_jpeg.pic.a)
common/libcamera_jpeg: CXX_STATIC_LIBRARY(common/libcamera_jpeg.pic.a)

libcamera_exif_OBJS = common/exif_utils.o
libcamera_exif_PC_DEPS := libchrome-$(BASE_VER) libexif
libcamera_exif_CPPFLAGS := $(call get_pc_cflags,$(libcamera_exif_PC_DEPS))
libcamera_exif_LDLIBS := $(call get_pc_libs,$(libcamera_exif_PC_DEPS))
CXX_LIBRARY(common/libcamera_exif.so): CPPFLAGS += $(libcamera_exif_CPPFLAGS)
CXX_LIBRARY(common/libcamera_exif.so): LDLIBS += $(libcamera_exif_LDLIBS)
CXX_LIBRARY(common/libcamera_exif.so): $(libcamera_exif_OBJS)
clean: CLEAN(common/libcamera_exif.so)
common/libcamera_exif: CXX_LIBRARY(common/libcamera_exif.so)

libcamera_timezone_OBJS = common/timezone.o
libcamera_timezone_PC_DEPS := libchrome-$(BASE_VER)
libcamera_timezone_CPPFLAGS := \
	$(call get_pc_cflags,$(libcamera_timezone_PC_DEPS))
libcamera_timezone_LDLIBS := $(call get_pc_libs,$(libcamera_timezone_PC_DEPS))
CXX_STATIC_LIBRARY(common/libcamera_timezone.pic.a): \
	CPPFLAGS += $(libcamera_timezone_CPPFLAGS)
CXX_STATIC_LIBRARY(common/libcamera_timezone.pic.a): \
	LDLIBS += $(libcamera_timezone_LDLIBS)
CXX_STATIC_LIBRARY(common/libcamera_timezone.pic.a): $(libcamera_timezone_OBJS)
clean: CLEAN(common/libcamera_timezone.pic.a)
common/libcamera_timezone: CXX_STATIC_LIBRARY(common/libcamera_timezone.pic.a)

libcamera_common_OBJS = common/future.o
libcamera_common_PC_DEPS := libchrome-$(BASE_VER)
libcamera_common_CPPFLAGS := \
	$(call get_pc_cflags,$(libcamera_common_PC_DEPS))
libcamera_common_LDLIBS := $(call get_pc_libs,$(libcamera_common_PC_DEPS))
CXX_STATIC_LIBRARY(common/libcamera_common.pic.a): \
	CPPFLAGS += $(libcamera_common_CPPFLAGS)
CXX_STATIC_LIBRARY(common/libcamera_common.pic.a): \
	LDLIBS += $(libcamera_common_LDLIBS)
CXX_STATIC_LIBRARY(common/libcamera_common.pic.a): $(libcamera_common_OBJS)
clean: CLEAN(common/libcamera_common.pic.a)
common/libcamera_common: CXX_STATIC_LIBRARY(common/libcamera_common.pic.a)

# To link against object files under common/, add $(COMMON_OBJECTS) to the
# dependency list of your target.
COMMON_OBJECTS := \
	common/future.o
