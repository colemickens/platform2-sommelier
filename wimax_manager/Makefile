# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for WiMAX Manager.

include common.mk

PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp

# Create a generator for DBus-C++ headers.
GEN_DBUSXX_ADAPTOR(%):
	$(call check_deps)
	$(call old_or_no_timestamp,\
           mkdir -p "$(dir $(TARGET_OR_MEMBER))" && \
           $(DBUSXX_XML2CPP) $< --adaptor=$(TARGET_OR_MEMBER))

GEN_DBUSXX_PROXY(%):
	$(call check_deps)
	$(call old_or_no_timestamp,\
           mkdir -p "$(dir $(TARGET_OR_MEMBER))" && \
           $(DBUSXX_XML2CPP) $< --proxy=$(TARGET_OR_MEMBER)) --sync

PROTOC ?= protoc
BASE_VER ?= 180609
PC_DEPS = dbus-c++-1 glib-2.0 gthread-2.0 gobject-2.0 \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -I$(SRC)/.. -I$(OUT) $(PC_CFLAGS)
LDLIBS += -lmetrics -lgdmwimax -lprotobuf $(PC_LIBS)

# TODO(benchan): Generalize and refactor DBus-C++ header generation.
GEN_DBUSXX_ADAPTOR(wimax_manager/dbus_bindings/manager.h): \
	$(SRC)/dbus_bindings/org.chromium.WiMaxManager.xml
clean: CLEAN(wimax_manager/dbus_bindings/manager.h)

GEN_DBUSXX_ADAPTOR(wimax_manager/dbus_bindings/device.h): \
	$(SRC)/dbus_bindings/org.chromium.WiMaxManager.Device.xml
clean: CLEAN(wimax_manager/dbus_bindings/device.h)

GEN_DBUSXX_ADAPTOR(wimax_manager/dbus_bindings/network.h): \
	$(SRC)/dbus_bindings/org.chromium.WiMaxManager.Network.xml
clean: CLEAN(wimax_manager/dbus_bindings/network.h)

GEN_DBUSXX_PROXY(wimax_manager/dbus_bindings/dbus_service.h): \
	$(SRC)/dbus_bindings/org.freedesktop.DBus.xml
clean: CLEAN(wimax_manager/dbus_bindings/dbus_service.h)

GEN_DBUSXX_PROXY(wimax_manager/dbus_bindings/power_manager.h): \
	$(SRC)/dbus_bindings/org.chromium.PowerManager.xml
clean: CLEAN(wimax_manager/dbus_bindings/power_manager.h)

LOCAL_PROTO_BINDINGS_DIR = wimax_manager/proto_bindings
LOCAL_PROTO_BINDINGS = \
	$(LOCAL_PROTO_BINDINGS_DIR)/config.pb.cc \
	$(LOCAL_PROTO_BINDINGS_DIR)/eap_parameters.pb.cc \
	$(LOCAL_PROTO_BINDINGS_DIR)/network_operator.pb.cc
LOCAL_PROTO_PATH = $(SRC)/proto
LOCAL_PROTO_HEADERS = $(patsubst %.cc,%.h,$(LOCAL_PROTO_BINDINGS))
LOCAL_PROTO_OBJS = $(patsubst %.cc,%.o,$(LOCAL_PROTO_BINDINGS))
$(LOCAL_PROTO_HEADERS): %.h: %.cc ;
$(LOCAL_PROTO_BINDINGS): \
	$(LOCAL_PROTO_BINDINGS_DIR)/%.pb.cc: $(LOCAL_PROTO_PATH)/%.proto
	$(call auto_mkdir,$@)
	$(PROTOC) --proto_path=$(LOCAL_PROTO_PATH) \
		--cpp_out=$(LOCAL_PROTO_BINDINGS_DIR) $<
clean: CLEAN($(LOCAL_PROTO_BINDINGS))
clean: CLEAN($(LOCAL_PROTO_HEADERS))
clean: CLEAN($(LOCAL_PROTO_OBJS))
$(eval $(call add_object_rules,$(LOCAL_PROTO_OBJS),CXX,cc))

SYSTEM_API_PROTO_BINDINGS = power_manager/suspend.pb.cc
SYSTEM_API_PROTO_PATH = $(SYSROOT)/usr/include/chromeos/dbus
SYSTEM_API_PROTO_HEADERS = $(patsubst %.cc,%.h,$(SYSTEM_API_PROTO_BINDINGS))
SYSTEM_API_PROTO_OBJS = $(patsubst %.cc,%.o,$(SYSTEM_API_PROTO_BINDINGS))
$(SYSTEM_API_PROTO_HEADERS): %.h: %.cc ;
$(SYSTEM_API_PROTO_BINDINGS): %.pb.cc: $(SYSTEM_API_PROTO_PATH)/%.proto
	$(PROTOC) --proto_path=$(SYSTEM_API_PROTO_PATH) --cpp_out=. $<
clean: CLEAN($(SYSTEM_API_PROTO_BINDINGS))
clean: CLEAN($(SYSTEM_API_PROTO_HEADERS))
clean: CLEAN($(SYSTEM_API_PROTO_OBJS))
$(eval $(call add_object_rules,$(SYSTEM_API_PROTO_OBJS),CXX,cc))

# Require DBus binding headers to be generated first.
$(patsubst %.o,%.o.depends,$(CXX_OBJECTS)): \
	GEN_DBUSXX_ADAPTOR(wimax_manager/dbus_bindings/device.h) \
	GEN_DBUSXX_ADAPTOR(wimax_manager/dbus_bindings/manager.h) \
	GEN_DBUSXX_ADAPTOR(wimax_manager/dbus_bindings/network.h) \
	GEN_DBUSXX_PROXY(wimax_manager/dbus_bindings/dbus_service.h) \
	GEN_DBUSXX_PROXY(wimax_manager/dbus_bindings/power_manager.h)

CXX_BINARY(wimax-manager): \
	$(filter-out testrunner.o %_unittest.o,$(CXX_OBJECTS)) \
	$(LOCAL_PROTO_OBJS) $(SYSTEM_API_PROTO_OBJS)
clean: CLEAN(wimax-manager)

testrunner: \
	$(filter-out %main.o,$(CXX_OBJECTS)) \
	$(LOCAL_PROTO_OBJS) $(SYSTEM_API_PROTO_OBJS)
	$(call cxx_binary, -lgtest -lgmock)
clean: CLEAN(testrunner)

default: CXX_BINARY(wimax-manager)

all: default

tests: TEST(testrunner)
