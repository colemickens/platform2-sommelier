# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

DBUSXX_XML2CPP ?= dbusxx-xml2cpp
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags dbus-c++-1 glib-2.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs dbus-c++-1 glib-2.0)

CXXFLAGS += -fvisibility=hidden
CXXFLAGS += -I$(OUT)		# for bindings

all: CXX_BINARY(debugd)

tests: override NEEDS_MOUNTS = 1
tests: TEST(CXX_BINARY(testrunner))

TOOLS := $(patsubst %,%_tool.o,debug_logs debug_mode modem_status \
           network_status ping route subprocess tracepath)
OBJS := debug_daemon.o process_with_id.o process_with_output.o $(TOOLS)
TESTOBJS := process_with_id_test.o

CXX_BINARY(debugd): main.o $(OBJS)
	$(call cxx_binary,-lminijail -lchromeos -lbase)
CXX_BINARY(testrunner): testrunner.o $(OBJS) $(TESTOBJS)
	$(call cxx_binary,-lgtest -lminijail -lchromeos -lbase)
clean: CXX_BINARY(debugd)

adaptors/%.h: ../share/%.xml
	mkdir -p $(dir $@)
	$(DBUSXX_XML2CPP) $^ --adaptor=$@

proxies/%.h: $(SYSROOT)/usr/share/dbus-1/interfaces/%.xml
	mkdir -p $(dir $@)
	$(DBUSXX_XML2CPP) $^ --proxy=$@

main.o.depends: adaptors/org.chromium.debugd.h
debug_daemon.o.depends: adaptors/org.chromium.debugd.h
debug_mode_tool.o.depends: proxies/org.chromium.flimflam.Manager.h \
                           proxies/org.freedesktop.DBus.Properties.h \
                           proxies/org.freedesktop.ModemManager.h
