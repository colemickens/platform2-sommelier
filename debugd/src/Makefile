# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

DBUSXX_XML2CPP ?= dbusxx-xml2cpp
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags dbus-c++-1 glib-2.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs dbus-c++-1 glib-2.0)

CXXFLAGS += -fvisibility=hidden
CXXFLAGS += -I$(OUT)		# for bindings

all: CXX_BINARY(debugd)

install:
	cp $(OUT)debugd ..

tests: override NEEDS_MOUNTS = 1
tests: TEST(CXX_BINARY(testrunner))

TOOLS:= $(patsubst %,%_tool.o,ping tracepath)
OBJS := debug_daemon.o process_with_id.o subprocess_tool.o $(TOOLS)
TESTOBJS := process_with_id_test.o

CXX_BINARY(debugd): main.o $(OBJS)
	$(call cxx_binary,-lminijail -lchromeos -lbase)
CXX_BINARY(testrunner): testrunner.o $(OBJS) $(TESTOBJS)
	$(call cxx_binary,-lgtest -lminijail -lchromeos -lbase)
clean: CXX_BINARY(debugd)

main.o: $(OUT)bindings/org.chromium.debugd.h
debug_daemon.o: $(OUT)bindings/org.chromium.debugd.h

$(OUT)bindings/%.h: share/%.xml
	mkdir -p $(dir $@)
	$(DBUSXX_XML2CPP) $^ --adaptor=$@
