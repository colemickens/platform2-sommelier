# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

PC_DEPS := cros-camera-android-headers
PC_CFLAGS := $(call get_pc_cflags,$(PC_DEPS))

CPPFLAGS += $(PC_CFLAGS) -D_GNU_SOURCE -I$(abspath $(SRC)) \
	    -I$(abspath $(SRC)/include)
CXXFLAGS += -std=c++17 -Wpointer-arith
CFLAGS += -std=c99

camera_hal_usb: hal/usb/camera_hal

.PHONY: camera_hal_usb

camera3_test: camera3_test/cros_camera_test

.PHONY: camera3_test

camera3_fuzzer: camera3_test/cros_camera_fuzzer

.PHONY: camera3_fuzzer

camera_profile: tools/generate_camera_profile

.PHONY: camera_profile

cros-camera-tool: tools/cros-camera-tool

.PHONY: cros-camera-tool

hal_adapter: hal_adapter/cros_camera_service

.PHONY: hal_adapter

libcab: common/libcab

.PHONY: libcab

libcab_test: common/libcab_test

.PHONY: libcab_test

libcamera_common: common/libcamera_common

.PHONY: libcamera_common

libcamera_exif: common/libcamera_exif

.PHONY: libcamera_exif

libcamera_ipc:
	$(MAKE) mojo/mojo_templates
	$(MAKE) common/libcamera_ipc

.PHONY: libcamera_ipc

libcamera_jpeg: common/libcamera_jpeg

.PHONY: libcamera_jpeg

libcamera_timezone: common/libcamera_timezone

.PHONY: libcamera_timezone

libcamera_v4l2_device: common/v4l2_device/libcamera_v4l2_device

.PHONY: libcamera_v4l2_device

libcbm: common/libcbm

.PHONY: libcbm

libjda: common/jpeg/libjda

.PHONY: libjda

libjda_test: common/jpeg/libjda_test

.PHONY: libjda_test
