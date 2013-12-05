# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_BINARY(tools/backlight_tool): \
	common/power_constants.o \
	tools/backlight_tool.o \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(tools/backlight_tool): CPPFLAGS += $(LIBCHROME_FLAGS)
CXX_BINARY(tools/backlight_tool): LDLIBS += $(LIBCHROME_LIBS) -lgflags
clean: CLEAN(tools/backlight_tool)
all: CXX_BINARY(tools/backlight_tool)

CXX_BINARY(tools/power_supply_info): \
	common/power_constants.o \
	tools/power_supply_info.o \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem_test.pie.a) \
	CXX_STATIC_LIBRARY(common/libprefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(tools/power_supply_info): CPPFLAGS += $(LIBCHROME_FLAGS)
CXX_BINARY(tools/power_supply_info): LDLIBS += $(LIBCHROME_LIBS) -lgflags
clean: CLEAN(tools/power_supply_info)
all: CXX_BINARY(tools/power_supply_info)

CXX_BINARY(tools/backlight_dbus_tool): \
	tools/backlight_dbus_tool.o \
	common/power_constants.o
CXX_BINARY(tools/backlight_dbus_tool): CPPFLAGS += $(LIBCHROME_FLAGS)
CXX_BINARY(tools/backlight_dbus_tool): LDLIBS += $(LIBCHROME_LIBS) -lgflags
clean: CXX_BINARY(tools/backlight_dbus_tool)
all: CXX_BINARY(tools/backlight_dbus_tool)

CXX_BINARY(tools/powerd_dbus_suspend): tools/powerd_dbus_suspend.o
CXX_BINARY(tools/powerd_dbus_suspend): CPPFLAGS += $(LIBCHROME_FLAGS)
CXX_BINARY(tools/powerd_dbus_suspend): LDLIBS += $(LIBCHROME_LIBS) -lgflags
clean: CXX_BINARY(tools/powerd_dbus_suspend)
all: CXX_BINARY(tools/powerd_dbus_suspend)

CXX_BINARY(tools/set_power_policy): \
	tools/set_power_policy.o \
	power_manager/policy.pb.o
CXX_BINARY(tools/set_power_policy): CPPFLAGS += $(LIBCHROME_FLAGS)
CXX_BINARY(tools/set_power_policy): \
	LDLIBS += $(LIBCHROME_LIBS) -lgflags -lprotobuf-lite
clean: CXX_BINARY(tools/set_power_policy)
all: CXX_BINARY(tools/set_power_policy)

CXX_BINARY(tools/get_powerd_initial_backlight_level): \
	tools/get_powerd_initial_backlight_level.o \
	CXX_STATIC_LIBRARY(powerd/libpolicy.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem_test.pie.a) \
	CXX_STATIC_LIBRARY(common/libprefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(tools/get_powerd_initial_backlight_level): \
	CPPFLAGS += $(LIBCHROME_FLAGS)
CXX_BINARY(tools/get_powerd_initial_backlight_level): \
	LDLIBS += $(LIBCHROME_LIBS)
clean: CXX_BINARY(tools/get_powerd_initial_backlight_level)
all: CXX_BINARY(tools/get_powerd_initial_backlight_level)

CXX_BINARY(tools/suspend_delay_sample): \
	tools/suspend_delay_sample.o \
	power_manager/suspend.pb.o
CXX_BINARY(tools/suspend_delay_sample): CPPFLAGS += $(LIBCHROME_FLAGS)
CXX_BINARY(tools/suspend_delay_sample): \
	LDLIBS += $(LIBCHROME_LIBS) -lgflags -lprotobuf-lite
clean: CXX_BINARY(tools/suspend_delay_sample)
all: CXX_BINARY(tools/suspend_delay_sample)

CXX_BINARY(tools/memory_suspend_test): tools/memory_suspend_test.o
CXX_BINARY(tools/memory_suspend_test): CPPFLAGS +=  $(LIBCHROME_FLAGS)
CXX_BINARY(tools/memory_suspend_test): \
	LDLIBS += $(LIBCHROME_LIBS) -lgflags
clean: CXX_BINARY(tools/memory_suspend_test)
all: CXX_BINARY(tools/memory_suspend_test)
