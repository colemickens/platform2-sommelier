# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for WiMAX Manager.

include common.mk

PKG_CONFIG ?= pkg-config

BASE_VER ?= 125070
PC_DEPS = dbus-c++-1 glib-2.0 gthread-2.0 gobject-2.0 \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -I$(SRC)/.. -I$(OUT) $(PC_CFLAGS)
LDLIBS += -lgflags -lmetrics $(PC_LIBS)

CXX_BINARY(wimax-manager): $(filter-out testrunner.o %_unittest.o,$(CXX_OBJECTS))
clean: CLEAN(wimax-manager)

testrunner: $(filter-out %main.o,$(CXX_OBJECTS))
	$(call cxx_binary, -lgtest -lgmock)
clean: CLEAN(testrunner)

default: CXX_BINARY(wimax-manager)

all: default

tests: TEST(testrunner)
