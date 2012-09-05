# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

BASE_VER ?= 125070
PKG_CONFIG ?= pkg-config

CXXFLAGS += -I$(SRC)/..

DBUS_DEPS = dbus-1 dbus-glib-1
DBUS_FLAGS := $(shell $(PKG_CONFIG) --cflags $(DBUS_DEPS))
DBUS_LIBS := $(shell $(PKG_CONFIG) --libs $(DBUS_DEPS))

GLIB_DEPS = glib-2.0 gobject-2.0 libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
GLIB_FLAGS := $(shell $(PKG_CONFIG) --cflags $(GLIB_DEPS))
GLIB_LIBS := $(shell $(PKG_CONFIG) --libs $(GLIB_DEPS))

COMMON_OBJS = \
  allow_usb_device_rule.o \
  deny_claimed_usb_device_rule.o \
  deny_uninitialized_device_rule.o \
  deny_usb_device_class_rule.o \
  deny_usb_vendor_id_rule.o \
  permission_broker.o \
  rule.o \
  udev_rule.o \
  usb_subsystem_udev_rule.o

PERMISSION_BROKER_FLAGS = $(DBUS_FLAGS) $(GLIB_FLAGS)
PERMISSION_BROKER_LIBS = $(DBUS_LIBS) $(GLIB_LIBS) -lgflags -ludev
CXX_BINARY(permission_broker): CXXFLAGS += $(PERMISSION_BROKER_FLAGS)
CXX_BINARY(permission_broker): LDLIBS += $(PERMISSION_BROKER_LIBS)
CXX_BINARY(permission_broker): \
  $(COMMON_OBJS) \
  permission_broker_main.o
clean: CLEAN(permission_broker)

PERMISSION_BROKER_UNITTEST_FLAGS = $(PERMISSION_BROKER_FLAGS)
PERMISSION_BROKER_UNITTEST_LIBS = $(PERMISSION_BROKER_LIBS) -lgmock -lgtest
CXX_BINARY(permission_broker_unittest): \
  CXXFLAGS += $(PERMISSION_BROKER_UNITTEST_FLAGS)
CXX_BINARY(permission_broker_unittest): \
  LDLIBS += $(PERMISSION_BROKER_UNITTEST_LIBS)
CXX_BINARY(permission_broker_unittest): \
  $(COMMON_OBJS) \
  allow_usb_device_rule_unittest.o \
  deny_claimed_usb_device_rule_unittest.o \
  deny_usb_device_class_rule_unittest.o \
  deny_usb_vendor_id_rule_unittest.o \
  permission_broker_unittest.o \
  run_all_tests.o
clean: CLEAN(permission_broker_unittest)

all: CXX_BINARY(permission_broker) CXX_BINARY(permission_broker_unittest)
