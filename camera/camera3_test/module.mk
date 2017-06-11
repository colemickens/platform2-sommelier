# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

### Rules to generate the arc_camera3_test binary.

camera3_test_PC_DEPS := gbm libcamera_client libcamera_metadata \
	libchrome-$(BASE_VER) libdrm libexif libsync libyuv
camera3_test_CPPFLAGS := $(call get_pc_cflags,$(camera3_test_PC_DEPS))
camera3_test_LDLIBS := $(call get_pc_libs,$(camera3_test_PC_DEPS)) -ldl \
	$(shell gtest-config --libs) -ljpeg
camera3_test_CXX_OBJECTS += \
	$(libcbm_OBJS) \
	common/future.o

CXX_BINARY(camera3_test/arc_camera3_test): CPPFLAGS += $(camera3_test_CPPFLAGS)
CXX_BINARY(camera3_test/arc_camera3_test): LDLIBS += $(camera3_test_LDLIBS)
CXX_BINARY(camera3_test/arc_camera3_test): \
	$(camera3_test_CXX_OBJECTS)

camera3_test/arc_camera3_test: CXX_BINARY(camera3_test/arc_camera3_test)

clean: CLEAN(camera3_test/arc_camera3_test)

.PHONY: camera3_test/arc_camera3_test
