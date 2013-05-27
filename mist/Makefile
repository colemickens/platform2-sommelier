# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for Chromium OS Modem Interface Switching Tool (mist).

include common.mk

PKG_CONFIG ?= pkg-config

BASE_VER ?= 180609
PC_DEPS = libchrome-$(BASE_VER) libchromeos-$(BASE_VER) libusb-1.0
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -I$(SRC)/.. $(PC_CFLAGS)
LDLIBS += $(PC_LIBS)

CXX_BINARY(mist): \
	$(filter-out testrunner.o %_unittest.o,$(CXX_OBJECTS))
clean: CLEAN(mist)

testrunner: \
	$(filter-out %main.o,$(CXX_OBJECTS))
	$(call cxx_binary, -lgtest -lgmock)
clean: CLEAN(testrunner)

default: CXX_BINARY(mist)

all: default

tests: TEST(testrunner)
