# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_BINARY(tools/backlight-tool): \
	tools/backlight_tool.o \
	CXX_STATIC_LIBRARY(powerm/libinternal_backlight.pie.a) \
	CXX_STATIC_LIBRARY(powerm/libexternal_backlight.pie.a)
CXX_BINARY(tools/backlight-tool): \
	CPPFLAGS += $(GLIB_FLAGS) $(DBUS_FLAGS)
CXX_BINARY(tools/backlight-tool): \
	LDLIBS += $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -ludev
clean: CLEAN(tools/backlight-tool)
all: CXX_BINARY(tools/backlight-tool)

CXX_BINARY(tools/power-supply-info): \
	tools/power_supply_info.o \
	CXX_STATIC_LIBRARY(powerd/libpower_supply.pie.a)
CXX_BINARY(tools/power-supply-info): CPPFLAGS += $(POWERSUPPLY_FLAGS)
CXX_BINARY(tools/power-supply-info): LDLIBS += $(POWERSUPPLY_LIBS)
clean: CLEAN(tools/power-supply-info)
all: CXX_BINARY(tools/power-supply-info)

power_state_tool.o.depends: power_state_control.pb.h
POWERSTATETOOL_DEPS = libchromeos-$(BASE_VER)
POWERSTATETOOL_FLAGS = $(GLIB_FLAGS) $(DBUS_FLAGS) \
                       $(shell $(PKG_CONFIG) --cflags $(POWERSTATETOOL_DEPS))
POWERSTATETOOL_LIBS = $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lprotobuf-lite \
                      $(shell $(PKG_CONFIG) --libs $(POWERSTATETOOL_DEPS))
POWERSTATETOOL_OBJS = power_state_control.pb.o tools/power_state_tool.o \
                      common/power_constants.o
CXX_BINARY(tools/power_state_tool): $(POWERSTATETOOL_OBJS) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(tools/power_state_tool): CPPFLAGS += $(POWERSTATETOOL_FLAGS)
CXX_BINARY(tools/power_state_tool): LDLIBS += $(POWERSTATETOOL_LIBS)
clean: CXX_BINARY(tools/power_state_tool)
all: CXX_BINARY(tools/power_state_tool)

BACKLIGHTDBUSTOOL_DEPS = libchromeos-$(BASE_VER)
BACKLIGHTDBUSTOOL_FLAGS = $(GLIB_FLAGS) $(DBUS_FLAGS) \
	$(shell $(PKG_CONFIG) --cflags $(POWERSTATETOOL_DEPS))
BACKLIGHTDBUSTOOL_LIBS = $(GLIB_LIBS) $(DBUS_LIBS) -lrt -lpthread -lgflags \
                         -lprotobuf-lite \
                          $(shell $(PKG_CONFIG) --libs $(POWERSTATETOOL_DEPS))
BACKLIGHTDBUSTOOL_OBJS = tools/backlight_dbus_tool.o common/power_constants.o
CXX_BINARY(tools/backlight_dbus_tool): $(BACKLIGHTDBUSTOOL_OBJS) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(tools/backlight_dbus_tool): CPPFLAGS += $(BACKLIGHTDBUSTOOL_FLAGS)
CXX_BINARY(tools/backlight_dbus_tool): LDLIBS += $(BACKLIGHTDBUSTOOL_LIBS)
clean: CXX_BINARY(tools/backlight_dbus_tool)
all: CXX_BINARY(tools/backlight_dbus_tool)

SUSPENDDELAYSAMPLE_FLAGS = $(LIBPOWERMAN_FLAGS)
SUSPENDDELAYSAMPLE_LIBS = $(LIBPOWERMAN_LIBS) -lgflags -lprotobuf-lite
SUSPENDDELAYSAMPLE_OBJS = tools/suspend_delay_sample.o \
                          common/power_constants.o common/util_dbus.o \
                          common/util_dbus_handler.o
CXX_BINARY(tools/suspend_delay_sample): $(SUSPENDDELAYSAMPLE_OBJS)
CXX_BINARY(tools/suspend_delay_sample): CPPFLAGS += $(SUSPENDDELAYSAMPLE_FLAGS)
CXX_BINARY(tools/suspend_delay_sample): LDLIBS += $(SUSPENDDELAYSAMPLE_LIBS)
clean: CXX_BINARY(tools/suspend_delay_sample)
all: CXX_BINARY(tools/suspend_delay_sample)

MEMORYSUSPENDTEST_DEPS = libchromeos-$(BASE_VER) libchrome-$(BASE_VER)
MEMORYSUSPENDTEST_FLAGS = $(shell $(PKG_CONFIG) --cflags \
                            $(MEMORYSUSPENDTEST_DEPS))
MEMORYSUSPENDTEST_LIBS = -lgflags \
                         $(shell $(PKG_CONFIG) --libs $(MEMORYSUSPENDTEST_DEPS))
MEMORYSUSPENDTEST_OBJS = tools/memory_suspend_test.o
CXX_BINARY(tools/memory_suspend_test): $(MEMORYSUSPENDTEST_OBJS)
CXX_BINARY(tools/memory_suspend_test): CPPFLAGS +=  $(MEMORYSUSPENDTEST_FLAGS)
CXX_BINARY(tools/memory_suspend_test): LDLIBS += $(MEMORYSUSPENDTEST_LIBS)
clean: CXX_BINARY(tools/memory_suspend_test)
all: CXX_BINARY(tools/memory_suspend_test)
