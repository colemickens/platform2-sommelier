# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

### Rules to generate the hal_adapter/arc_camera3_service binary.

hal_adapter_PC_DEPS := \
	libbrillo-$(BASE_VER) libcamera_metadata libdrm libmojo-$(BASE_VER) \
	libsync
hal_adapter_CPPFLAGS := $(call get_pc_cflags,$(hal_adapter_PC_DEPS))
hal_adapter_LDLIBS := $(call get_pc_libs,$(hal_adapter_PC_DEPS)) -ldl

CXX_BINARY(hal_adapter/arc_camera3_service): CPPFLAGS += $(hal_adapter_CPPFLAGS)
CXX_BINARY(hal_adapter/arc_camera3_service): LDLIBS += $(hal_adapter_LDLIBS)
CXX_BINARY(hal_adapter/arc_camera3_service): \
	$(ANDROID_OBJECTS) \
	$(COMMON_OBJECTS) \
	$(hal_adapter_CXX_OBJECTS) \
	$(hal_adapter_mojo_CXX_OBJECTS)

hal_adapter/arc_camera3_service: CXX_BINARY(hal_adapter/arc_camera3_service)

clean: CLEAN(hal_adapter/arc_camera3_service)

.PHONY: hal_adapter/arc_camera3_service
