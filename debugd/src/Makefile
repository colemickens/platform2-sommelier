# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

DBUSXX_XML2CPP ?= dbusxx-xml2cpp
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags dbus-c++-1)
LDFLAGS += $(shell $(PKG_CONFIG) --libs dbus-c++-1)

CXXFLAGS += -fvisibility=hidden
CXXFLAGS += -I$(OUT)		# for bindings

all: debugd

install:
	cp $(OUT)debugd .

RM_ON_CLEAN += debugd
debugd: $(OUT)debugd

$(OUT)debugd: $(OUT)debugdaemon.o $(OUT)main.o
	$(call cxx_binary,-lminijail)

$(OUT)main.o: $(OUT)bindings/org.chromium.debugd.h
$(OUT)debugdaemon.o: $(OUT)bindings/org.chromium.debugd.h

RM_ON_CLEAN += bindings/org.chromium.debugd.h
$(OUT)bindings/%.h: ../share/%.xml
	mkdir -p $(dir $@)
	$(DBUSXX_XML2CPP) $^ --adaptor=$@
