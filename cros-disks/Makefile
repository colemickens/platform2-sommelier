# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for cros-disks.

# Pull in chromium os defaults
# TODO(rtc): This verison of common.mk came from drewry@, we should find
# a way to share the file rather than copying it.
include common.mk

PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp

# Create a generator for DBUS-C++ headers.
# TODO(wad,benchan): integrate into common.mk.
GEN_DBUSXX(%):
	$(call check_deps)
	$(call old_or_no_timestamp,\
           mkdir -p "$(dir $(TARGET_OR_MEMBER))" && \
           $(DBUSXX_XML2CPP) $< --adaptor=$(TARGET_OR_MEMBER))

PC_DEPS = dbus-c++-1 glib-2.0 libchromeos
INCLUDE_DIRS = -I$(SRC)/include -I$(SRC)/.. -I$(SRC) -I$(OUT) \
	-I$(OUT)cros-disks \
	$(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
LIB_DIRS = $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CFLAGS := $(CFLAGS)
CXXFLAGS := $(INCLUDE_DIRS) $(CXXFLAGS)
LDFLAGS += -lbase -lblkid -lgflags -lmetrics -lminijail -lparted \
	-lrootdev -ludev $(LIB_DIRS)

GEN_DBUSXX(cros-disks/cros-disks-server.h): $(SRC)/cros-disks.xml
cros-disks/cros-disks-server.h: GEN_DBUSXX(cros-disks/cros-disks-server.h)
clean: CLEAN(cros-disks/cros-disks-server.h)

# Require the header to be generated first.
$(patsubst %.o,%.o.depends,$(CXX_OBJECTS)): cros-disks/cros-disks-server.h

CXX_BINARY(disks): $(filter-out %_testrunner.o %_unittest.o,$(CXX_OBJECTS))
clean: CLEAN(disks)

disks_testrunner: $(filter-out %main.o,$(CXX_OBJECTS))
	$(call cxx_binary, -lgtest -lgmock)
clean: CLEAN(disks_testrunner)

# Some shortcuts
disks: CXX_BINARY(disks)
all: disks


# Define default (host and qemu arguments)
root_tests: override GTEST_ARGS := --gtest_filter='*.RunAsRoot*'
# Override the defaults if we're testing in arm under qemu.
root_tests: override GTEST_ARGS.qemu.arm := --gtest_filter='*.RunAsRoot*-DiskManagerTest.*:ExternalMounterTest.*:UdevDeviceTest.*:MountInfoTest.RetrieveFromCurrentProcess'
root_tests: override NEEDS_ROOT = 1
root_tests: override NEEDS_MOUNTS = 1
root_tests: TEST(disks_testrunner)

user_tests: override GTEST_ARGS := --gtest_filter=-'*.RunAsRoot*'
# qemu-arm doesn't allow subprocesses and may miss some other pieces.
user_tests: override GTEST_ARGS.qemu.arm := --gtest_filter='-*.RunAsRoot*:DiskManagerTest.*:ExternalMounterTest.*:UdevDeviceTest.*:MountInfoTest.RetrieveFromCurrentProcess'
user_tests: TEST(disks_testrunner)
.PHONY: user_tests root_tests

tests:
	$(MAKE) user_tests
	$(MAKE) root_tests # NEEDS_ROOT=1 NEEDS_MOUNTS=1
