# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for chaps.

# Pull in chromium os defaults
include common.mk

PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp
PROTOC = protoc

PC_DEPS = dbus-c++-1 libchromeos protobuf
INCLUDE_DIRS = -I$(OUT)include -I.. $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
LIB_DIRS = -L$(OUT) $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS := $(INCLUDE_DIRS) $(CXXFLAGS)
LDFLAGS += -lbase -lchromeos $(LIB_DIRS)

# Rules
chaps_proxy_generated.h: chaps_interface.xml
	$(DBUSXX_XML2CPP) $< --proxy=$@
RM_ON_CLEAN += chaps_proxy_generated.h

chaps_adaptor_generated.h: chaps_interface.xml
	$(DBUSXX_XML2CPP) $< --adaptor=$@
RM_ON_CLEAN += chaps_adaptor_generated.h

attributes.pb.h: attributes.proto
	$(PROTOC) -I. --cpp_out=. $<
RM_ON_CLEAN += attributes.pb.h attributes.pb.cc

attributes.pb.cc: attributes.pb.h
	@echo

attributes.cc: attributes.pb.h

chaps_adaptor.cc chapsd.cc: chaps_adaptor_generated.h
chaps_proxy.cc chaps.cc: chaps_proxy_generated.h

$(OUT)libchaps.so: $(OUT)chaps.o \
                   $(OUT)chaps_proxy.o \
                   $(OUT)chaps_utility.o \
                   $(OUT)attributes.pb.o \
                   $(OUT)attributes.o
	$(call cxx_library)
RM_ON_CLEAN += $(OUT)libchaps.so

$(OUT)chapsd: $(OUT)chapsd.o \
              $(OUT)chaps_service.o \
              $(OUT)chaps_service_redirect.o \
              $(OUT)chaps_adaptor.o \
              $(OUT)chaps_utility.o \
              $(OUT)attributes.pb.o \
              $(OUT)attributes.o
	$(call cxx_binary, -ldl)
RM_ON_CLEAN += $(OUT)chapsd

$(OUT)chapsd_test: $(OUT)chapsd_test.o $(OUT)chaps_service_redirect.o
	$(call cxx_binary, -lgtest -lgflags -lchaps -ldl)
RM_ON_CLEAN += $(OUT)chapsd_test

$(OUT)chaps_test: $(OUT)chaps_test.o $(OUT)libchaps.so
	$(call cxx_binary, -lgtest -lgmock)
RM_ON_CLEAN += $(OUT)chaps_test

$(OUT)chaps_live_test: $(OUT)chaps_test.o
	$(call cxx_binary, -lgtest -lgmock -lchaps)
RM_ON_CLEAN += $(OUT)chaps_live_test

$(OUT)vpn_replay: $(OUT)vpn_replay.o
	$(call cxx_binary, -lchaps)
RM_ON_CLEAN += $(OUT)vpn_replay

$(OUT)chaps_event_generator: $(OUT)chaps_event_generator.o \
                             $(OUT)chaps_proxy.o \
                             $(OUT)chaps_utility.o \
                             $(OUT)attributes.pb.o \
                             $(OUT)attributes.o
	$(call cxx_binary)
RM_ON_CLEAN += $(OUT)chaps_event_generator

$(OUT)chaps_service_test: $(OUT)chaps_service_test.o \
                          $(OUT)chaps_service.o \
                          $(OUT)attributes.pb.o \
                          $(OUT)attributes.o \
                          $(OUT)chaps_utility.o
	$(call cxx_binary, -lgtest -lgmock)
RM_ON_CLEAN += $(OUT)chaps_service_test

# Some shortcuts
all: $(OUT)libchaps.so $(OUT)chapsd

lib: $(OUT)libchaps.so

chapsd: $(OUT)chapsd
	@echo

chapsd_test: $(OUT)chapsd_test
	@echo

dbus-headers: chaps_adaptor_generated.h \
              chaps_proxy_generated.h

protobufs: attributes.pb.h

tests: $(OUT)chaps_test $(OUT)chaps_service_test

live_tests: $(OUT)chapsd_test $(OUT)chaps_event_generator

perf: $(OUT)vpn_replay

runtests: tests
	$(OUT)chaps_test
	$(OUT)chaps_service_test
# This will have to wait until it does not depend on opencryptoki.
# For now this test must be run on a live system.
#LD_LIBRARY_PATH=$(OUT) "$(OUT)chapsd_test"
