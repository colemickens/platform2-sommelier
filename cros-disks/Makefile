# Copyright (C) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE.makefile file.
#
# Top-level Makefile for cros-disks.

# Pull in chromium os defaults
# TODO(rtc): This verison of common.mk came from drewry@, we should find
# a way to share the file rather than copying it.
include common.mk

PKG_CONFIG ?= pkg-config

INCLUDE_DIRS = -I.. $(shell $(PKG_CONFIG) --cflags dbus-1 dbus-glib-1\
	dbus-c++-1 glib-2.0)
LIB_DIRS = $(shell $(PKG_CONFIG) --libs dbus-1 dbus-glib-1 dbus-c++-1 glib-2.0)

CFLAGS := -Iinclude $(CFLAGS)
CXXFLAGS := -Iinclude -I../ $(INCLUDE_DIRS) $(CXXFLAGS)
LDFLAGS += -lbase -lgflags -lmetrics $(LIB_DIRS)

disks: $(OUT)disks

$(OUT)disks: $(filter-out %_testrunner.o %_unittest.o,$(C_OBJECTS)) \
              $(CXX_OBJECTS)
	$(call cxx_binary)
all: $(OUT)disks
RM_ON_CLEAN += $(OUT)disks
