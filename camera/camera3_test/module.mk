# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

### Rules to generate the cros_camera_test binary.

camera3_test_PC_DEPS := gbm libcamera_client libcamera_common \
	libcamera_metadata libcbm libchrome-$(BASE_VER) libdrm libexif libsync libyuv
camera3_test_CPPFLAGS := $(call get_pc_cflags,$(camera3_test_PC_DEPS))
camera3_test_LDLIBS := $(call get_pc_libs,$(camera3_test_PC_DEPS)) -ldl \
	-Wl,-Bstatic -lgtest -Wl,-Bdynamic -ljpeg -lpthread -pthread

CXX_BINARY(camera3_test/cros_camera_test): CPPFLAGS += $(camera3_test_CPPFLAGS)
CXX_BINARY(camera3_test/cros_camera_test): LDLIBS += $(camera3_test_LDLIBS)
CXX_BINARY(camera3_test/cros_camera_test): \
	$(camera3_test_CXX_OBJECTS) \
	common/utils/camera_hal_enumerator.o

camera3_test/cros_camera_test: CXX_BINARY(camera3_test/cros_camera_test)

clean: CLEAN(camera3_test/cros_camera_test)

.PHONY: camera3_test/cros_camera_test

CXX_BINARY(camera3_test/cros_camera_fuzzer): CPPFLAGS += \
	$(camera3_test_CPPFLAGS) -g -D FUZZER \
	-fsanitize=address -fsanitize-coverage=trace-pc-guard
CXX_BINARY(camera3_test/cros_camera_fuzzer): LDLIBS += $(camera3_test_LDLIBS) \
	-lFuzzer -fsanitize=address -fsanitize-coverage=trace-pc-guard
CXX_BINARY(camera3_test/cros_camera_fuzzer): \
	$(camera3_test_CXX_OBJECTS)

camera3_test/cros_camera_fuzzer: CXX_BINARY(camera3_test/cros_camera_fuzzer)

clean: CLEAN(camera3_test/cros_camera_fuzzer)

.PHONY: camera3_test/cros_camera_fuzzer
