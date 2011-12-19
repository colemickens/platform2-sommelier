# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

DBUSXX_XML2CPP ?= dbusxx-xml2cpp
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags dbus-c++-1 glib-2.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs dbus-c++-1 glib-2.0)

CXXFLAGS += -fvisibility=hidden
CXXFLAGS += -I$(OUT)		# for bindings

all: $(OUT)debugd

install:
	cp $(OUT)debugd ..

small_tests: $(OUT)testrunner

RM_ON_CLEAN += debugd

TOOLS:= $(patsubst %,%_tool.o,ping)
OBJS := $(patsubst %,$(OUT)%,debug_daemon.o process_with_id.o subprocess_tool.o $(TOOLS))
TESTOBJS := $(patsubst %,$(OUT)%,process_with_id_test.o)

$(OUT)debugd: $(OUT)main.o $(OBJS)
	$(call cxx_binary,-lminijail -lchromeos -lbase)
$(OUT)testrunner: $(OUT)testrunner.o $(OBJS) $(TESTOBJS)
	$(call cxx_binary,-lgtest -lminijail -lchromeos -lbase)

$(OUT)main.o: $(OUT)bindings/org.chromium.debugd.h
$(OUT)debug_daemon.o: $(OUT)bindings/org.chromium.debugd.h

RM_ON_CLEAN += bindings/org.chromium.debugd.h
$(OUT)bindings/%.h: ../share/%.xml
	mkdir -p $(dir $@)
	$(DBUSXX_XML2CPP) $^ --adaptor=$@
