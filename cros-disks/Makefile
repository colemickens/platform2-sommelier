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

INCLUDE_DIRS = -I.. -I$(OUT) $(shell $(PKG_CONFIG) --cflags dbus-1 dbus-glib-1\
	dbus-c++-1 glib-2.0)
LIB_DIRS = $(shell $(PKG_CONFIG) --libs dbus-1 dbus-glib-1 dbus-c++-1 glib-2.0)

CFLAGS := -Iinclude $(CFLAGS)
CXXFLAGS := -Iinclude -I../ $(INCLUDE_DIRS) $(CXXFLAGS)
LDFLAGS += -lbase -lblkid -lchromeos -lgflags -lmetrics -lminijail -lparted \
	-lrootdev -ludev $(LIB_DIRS)

cros-disks-server.h: cros-disks.xml
	$(DBUSXX_XML2CPP) cros-disks.xml --adaptor=$@
RM_ON_CLEAN += cros-disks-server.h

$(OUT)disks: $(filter-out %_testrunner.o %_unittest.o,$(CXX_OBJECTS)) \
		cros-disks-server.h
	$(call cxx_binary)
RM_ON_CLEAN += $(OUT)disks

$(OUT)disks_testrunner: $(filter-out %main.o,$(CXX_OBJECTS)) \
		cros-disks-server.h
	$(call cxx_binary, -lgtest -lgmock)
RM_ON_CLEAN += $(OUT)disks_testrunner

# Some shortcuts
all: disks

disks: dbus-headers
	$(MAKE) $(OUT)disks

dbus-headers: cros-disks-server.h

tests: dbus-headers
	$(MAKE) $(OUT)disks_testrunner

runtests: tests
	"$(OUT)disks_testrunner" --gtest_filter='-*.RunAsRoot*'
	sudo LD_LIBRARY_PATH="${LD_LIBRARY_PATH}" \
		"$(OUT)disks_testrunner" --gtest_filter='*.RunAsRoot*'
