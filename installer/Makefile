# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OUT ?= $(PWD)/build

include common.mk

BASE_VER ?= 307740
PC_DEPS = libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += \
	-DCHROMEOS_ENVIRONMENT \
	-I$(SRC)/.. \
	-I$(SRC)/../verity/include

LDLIBS += -lvboot_host -ldm-bht

CXX_STATIC_BINARY(cros_installer): \
		$(C_OBJECTS) \
		$(filter-out testrunner.o %_unittest.o %_main.o \
			nand_%.o, $(CXX_OBJECTS)) \
		cros_installer_main.o

CXX_BINARY(nand_partition): \
		nand_partition.o \
		nand_partition_main.o
CXX_BINARY(nand_partition): CXXFLAGS += $(PC_CFLAGS) -std=c++11
CXX_BINARY(nand_partition): LDLIBS += $(PC_LIBS)

clean: CLEAN(cros_installer)
all: CXX_STATIC_BINARY(cros_installer)
cros_installer: CXX_STATIC_BINARY(cros_installer)
nand_partition: CXX_BINARY(nand_partition)

UNITTEST_LIBS := $(shell gmock-config --libs) $(shell gtest-config --libs)
CXX_STATIC_BINARY(cros_installer_unittest): LDLIBS += $(UNITTEST_LIBS)
CXX_STATIC_BINARY(cros_installer_unittest): \
		$(C_OBJECTS) \
		$(filter-out %_main.o nand_%.o, $(CXX_OBJECTS))

clean: CLEAN(cros_installer_unittest)
tests: TEST(CXX_STATIC_BINARY(cros_installer_unittest))
