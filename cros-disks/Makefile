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
DBUSXX_XML2CPP = dbusxx-xml2cpp

INCLUDE_DIRS = -I.. -I$(OUT)include $(shell $(PKG_CONFIG) --cflags dbus-1 dbus-glib-1\
	dbus-c++-1 glib-2.0)
LIB_DIRS = $(shell $(PKG_CONFIG) --libs dbus-1 dbus-glib-1 dbus-c++-1 glib-2.0)

CFLAGS := -Iinclude $(CFLAGS)
CXXFLAGS := -Iinclude -I../ $(INCLUDE_DIRS) $(CXXFLAGS)
LDFLAGS += -lbase -lgflags -lmetrics -ludev $(LIB_DIRS)

$(OUT)include/cros-disks-server.h: cros-disks.xml
	mkdir -p $(OUT)include
	$(DBUSXX_XML2CPP) cros-disks.xml --adaptor=$@
RM_ON_CLEAN += $(OUT)include/cros-disks-server.h

$(OUT)disks: $(filter-out %_testrunner.o %_unittest.o,$(C_OBJECTS)) \
              $(OUT)include/cros-disks-server.h \
              $(CXX_OBJECTS)
	$(call cxx_binary)
all: $(OUT)disks
RM_ON_CLEAN += $(OUT)disks

# Some shortcuts
disks: $(OUT)disks
dbus-headers: $(OUT)include/cros-disks-server.h
