# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OUT ?= $(PWD)/build

include common.mk

CXXFLAGS += -DCHROMEOS_ENVIRONMENT

LDFLAGS += -lvboot_host -ldm-bht

CXX_STATIC_BINARY(cros_installer): \
		$(C_OBJECTS) \
		$(filter-out testrunner.o %_unittest.o,$(CXX_OBJECTS))

clean: CLEAN(cros_installer)
all: CXX_STATIC_BINARY(cros_installer)
cros_installer: CXX_STATIC_BINARY(cros_installer)

UNITTEST_LIBS := $(shell gmock-config --libs) $(shell gtest-config --libs)
CXX_BINARY(cros_installer_unittest): LDFLAGS += $(UNITTEST_LIBS)
CXX_BINARY(cros_installer_unittest): \
		$(C_OBJECTS) \
		$(filter-out %_main.o,$(CXX_OBJECTS))

clean: CLEAN(cros_installer_unittest)
all: CXX_BINARY(cros_installer_unittest)
tests: TEST(CXX_BINARY(cros_installer_unittest))
