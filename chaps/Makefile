# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for chaps.

# Pull in chromium os defaults
include common.mk

PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp

INCLUDE_DIRS = -I$(OUT)include -I.. \
               $(shell $(PKG_CONFIG) --cflags dbus-1 dbus-c++-1)
LIB_DIRS = -L$(OUT) $(shell $(PKG_CONFIG) --libs dbus-1 dbus-c++-1)

CXXFLAGS := $(INCLUDE_DIRS) $(CXXFLAGS)
LDFLAGS += -lbase $(LIB_DIRS)

# Rules
$(OUT)include/chaps/chaps_proxy_generated.h: chaps_interface.xml
	@mkdir -p $(dir $@)
	@$(DBUSXX_XML2CPP) $< --proxy=$@
RM_ON_CLEAN += $(OUT)include/chaps/chaps_proxy_generated.h
RMDIR_ON_CLEAN += $(OUT)include $(OUT)include/chaps

$(OUT)include/chaps/chaps_adaptor_generated.h: chaps_interface.xml
	@mkdir -p $(dir $@)
	@$(DBUSXX_XML2CPP) $< --adaptor=$@
RM_ON_CLEAN += $(OUT)include/chaps/chaps_adaptor_generated.h

$(OUT)libchaps.so: dbus-headers $(OUT)chaps.o \
                                $(OUT)chaps_proxy.o \
                                $(OUT)chaps_utility.o
	$(call cxx_library)
RM_ON_CLEAN += $(OUT)libchaps.so

$(OUT)chapsd: dbus-headers $(OUT)chapsd.o \
                           $(OUT)chaps_service_redirect.o \
                           $(OUT)chaps_adaptor.o \
                           $(OUT)chaps_utility.o
	$(call cxx_binary, -ldl)
RM_ON_CLEAN += $(OUT)chapsd

$(OUT)chapsd_test: $(OUT)chapsd_test.o $(OUT)chaps_service_redirect.o
	$(call cxx_binary, -lgtest -lgflags -lchaps -ldl)
RM_ON_CLEAN += $(OUT)chapsd_test

$(OUT)chaps_test: $(OUT)chaps_test.o
	$(call cxx_binary, -lgtest -lgmock -lchaps)
RM_ON_CLEAN += $(OUT)chaps_test

# Some shortcuts
all: $(OUT)libchaps.so $(OUT)chapsd $(OUT)chaps_test $(OUT)chapsd_test

dbus-headers: $(OUT)include/chaps/chaps_adaptor_generated.h \
              $(OUT)include/chaps/chaps_proxy_generated.h

tests: $(OUT)libchaps.so $(OUT)chaps_test $(OUT)chapsd_test

runtests: tests
	LD_LIBRARY_PATH=$(OUT) "$(OUT)chaps_test"
# This will have to wait until it does not depend on opencryptoki.
# For now this test must be run on a live system.
#LD_LIBRARY_PATH=$(OUT) "$(OUT)chapsd_test"
