# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_BINARY(tools/backlight_tool): \
	common/power_constants.o \
	tools/backlight_tool.o \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(tools/backlight_tool): \
	CPPFLAGS += $(GLIB_FLAGS) $(DBUS_FLAGS)
CXX_BINARY(tools/backlight_tool): \
	LDLIBS += $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -ludev
clean: CLEAN(tools/backlight_tool)
all: CXX_BINARY(tools/backlight_tool)

CXX_BINARY(tools/power_supply_info): \
	common/power_constants.o \
	tools/power_supply_info.o \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(common/libprefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(tools/power_supply_info): CPPFLAGS += $(GLIB_FLAGS)
CXX_BINARY(tools/power_supply_info): LDLIBS += $(GLIB_LIBS) -lgflags
clean: CLEAN(tools/power_supply_info)
all: CXX_BINARY(tools/power_supply_info)

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

CXX_BINARY(tools/powerd_dbus_suspend): tools/powerd_dbus_suspend.o
CXX_BINARY(tools/powerd_dbus_suspend): CPPFLAGS += $(DBUS_FLAGS) \
	$(shell $(PKG_CONFIG) --cflags libchrome-$(BASE_VER))
CXX_BINARY(tools/powerd_dbus_suspend): LDLIBS += $(DBUS_LIBS) -lgflags \
	$(shell $(PKG_CONFIG) --libs libchrome-$(BASE_VER))
clean: CXX_BINARY(tools/powerd_dbus_suspend)
all: CXX_BINARY(tools/powerd_dbus_suspend)

SET_POWER_POLICY_DEPS = libchromeos-$(BASE_VER) libchrome-$(BASE_VER)
CXX_BINARY(tools/set_power_policy): \
	tools/set_power_policy.o \
	power_manager/policy.pb.o \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(tools/set_power_policy): CPPFLAGS += \
	$(shell $(PKG_CONFIG) --cflags $(SET_POWER_POLICY_DEPS)) $(DBUS_FLAGS)
CXX_BINARY(tools/set_power_policy): LDLIBS += -lgflags -lprotobuf-lite \
	$(shell $(PKG_CONFIG) --libs $(SET_POWER_POLICY_DEPS)) $(DBUS_LIBS)
clean: CXX_BINARY(tools/set_power_policy)
all: CXX_BINARY(tools/set_power_policy)

CXX_BINARY(tools/suspend_delay_sample): \
	tools/suspend_delay_sample.o \
	common/dbus_handler.o \
	common/power_constants.o \
	common/util_dbus.o \
	power_manager/suspend.pb.o
CXX_BINARY(tools/suspend_delay_sample): CPPFLAGS += $(LIBPOWERMAN_FLAGS) \
	$(DBUS_FLAGS) $(GLIB_FLAGS)
CXX_BINARY(tools/suspend_delay_sample): LDLIBS += $(LIBPOWERMAN_LIBS) \
	$(DBUS_LIBS) $(GLIB_LIBS) -lgflags -lprotobuf-lite
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
