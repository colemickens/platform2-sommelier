# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for cros_boot_mode.

# Pull in chromium os defaults
include common.mk

BASE_VER = 85268
PC_DEPS = libchrome-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += $(PC_CFLAGS)

# Filter out the two legacy style binaries.
BOOT_MODE_OBJECTS=$(filter-out %_testrunner.o %_unittest.o,$(CXX_OBJECTS))
$(OUT)cros_boot_mode: $(BOOT_MODE_OBJECTS)
	$(call cxx_binary)
all: $(OUT)cros_boot_mode
RM_ON_CLEAN += $(OUT)cros_boot_mode

$(OUT)libcros_boot_mode.so: $(filter-out %_main.o,$(BOOT_MODE_OBJECTS))
	$(call cc_library)
all: $(OUT)libcros_boot_mode.so
RM_ON_CLEAN += $(OUT)libcros_boot_mode.so

#
# Tests are not built by default.
#

$(OUT)cros_boot_mode_testrunner: $(filter-out %_main.o,$(CXX_OBJECTS))
	$(call cxx_binary,-lgtest -lgmock $(PC_LIBS))
RM_ON_CLEAN += $(OUT)cros_boot_mode_testrunner
# Will run each prerequisite for tests.
# If cross-compiled, this is done against the correct architecture.
small_tests: $(OUT)cros_boot_mode_testrunner
# No large tests here so we use the NONE target.
large_tests: NONE
