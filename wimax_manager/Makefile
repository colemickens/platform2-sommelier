# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for WiMAX Manager.

include common.mk

PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp

# Create a generator for DBus-C++ headers.
GEN_DBUSXX(%):
	$(call check_deps)
	$(call old_or_no_timestamp,\
           mkdir -p "$(dir $(TARGET_OR_MEMBER))" && \
           $(DBUSXX_XML2CPP) $< --adaptor=$(TARGET_OR_MEMBER))

BASE_VER ?= 125070
PC_DEPS = dbus-c++-1 glib-2.0 gthread-2.0 gobject-2.0 \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -I$(SRC)/.. -I$(OUT) $(PC_CFLAGS)
LDLIBS += -lgflags -lmetrics -lgdmwimax -lz $(PC_LIBS)

# TODO(benchan): Generalize and refactor DBus-C++ header generation.
GEN_DBUSXX(wimax_manager/dbus_bindings/manager.h): \
	$(SRC)/dbus_bindings/org.chromium.WiMaxManager.xml
clean: CLEAN(wimax_manager/dbus_bindings/manager.h)

GEN_DBUSXX(wimax_manager/dbus_bindings/device.h): \
	$(SRC)/dbus_bindings/org.chromium.WiMaxManager.Device.xml
clean: CLEAN(wimax_manager/dbus_bindings/device.h)

# Require DBus binding headers to be generated first.
$(patsubst %.o,%.o.depends,$(CXX_OBJECTS)): \
	GEN_DBUSXX(wimax_manager/dbus_bindings/manager.h) \
	GEN_DBUSXX(wimax_manager/dbus_bindings/device.h)

CXX_BINARY(wimax-manager): $(filter-out testrunner.o %_unittest.o,$(CXX_OBJECTS))
clean: CLEAN(wimax-manager)

testrunner: $(filter-out %main.o,$(CXX_OBJECTS))
	$(call cxx_binary, -lgtest -lgmock)
clean: CLEAN(testrunner)

default: CXX_BINARY(wimax-manager)

all: default

tests: TEST(testrunner)
