# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for Chromium OS Modem Interface Switching Tool (mist).

include common.mk

PKG_CONFIG ?= pkg-config

PROTOC ?= protoc
BASE_VER ?= 180609
PC_DEPS = libchrome-$(BASE_VER) libchromeos-$(BASE_VER) libusb-1.0
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

GENERATED_DIR = generated
CPPFLAGS += -I$(SRC)/.. -I$(OUT)/$(GENERATED_DIR) $(PC_CFLAGS)
LDLIBS += -lprotobuf -lpthread -ludev $(PC_LIBS)

MIST_PROTO_BINDINGS_DIR = $(GENERATED_DIR)/mist/proto_bindings
MIST_PROTO_BINDINGS = \
	$(MIST_PROTO_BINDINGS_DIR)/config.pb.cc \
	$(MIST_PROTO_BINDINGS_DIR)/usb_modem_info.pb.cc
MIST_PROTO_PATH = $(SRC)/proto
MIST_PROTO_HEADERS = $(patsubst %.cc,%.h,$(MIST_PROTO_BINDINGS))
MIST_PROTO_OBJS = $(patsubst %.cc,%.o,$(MIST_PROTO_BINDINGS))
$(MIST_PROTO_HEADERS): %.h: %.cc ;
$(MIST_PROTO_BINDINGS): \
	$(MIST_PROTO_BINDINGS_DIR)/%.pb.cc: $(MIST_PROTO_PATH)/%.proto
	$(call auto_mkdir,$@)
	$(PROTOC) --proto_path=$(MIST_PROTO_PATH) \
		--cpp_out=$(MIST_PROTO_BINDINGS_DIR) $<
clean: CLEAN($(MIST_PROTO_BINDINGS))
clean: CLEAN($(MIST_PROTO_HEADERS))
clean: CLEAN($(MIST_PROTO_OBJS))
$(eval $(call add_object_rules,$(MIST_PROTO_OBJS),CXX,cc))

CXX_BINARY(mist): \
	$(filter-out mock_%.o testrunner.o %_unittest.o,$(CXX_OBJECTS)) \
	$(MIST_PROTO_OBJS)
clean: CLEAN(mist)

testrunner: \
	$(filter-out %main.o,$(CXX_OBJECTS)) \
	$(MIST_PROTO_OBJS)
	$(call cxx_binary, -lgtest -lgmock)
clean: CLEAN(testrunner)

default: CXX_BINARY(mist)

all: default

tests: TEST(testrunner)
