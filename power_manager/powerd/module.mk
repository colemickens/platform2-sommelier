# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

POWERSUPPLY_FLAGS = $(GLIB_FLAGS)
POWERSUPPLY_LIBS = $(GLIB_LIBS) -lgflags
LIBPOWER_SUPPLY_OBJS = \
	common/inotify.o \
	common/power_constants.o \
	common/power_prefs.o \
	powerd/power_supply.o
CXX_STATIC_LIBRARY(powerd/libpower_supply.pie.a): $(LIBPOWER_SUPPLY_OBJS)
CXX_STATIC_LIBRARY(powerd/libpower_supply.pie.a): \
	CPPFLAGS += $(POWERSUPPLY_FLAGS)
CXX_STATIC_LIBRARY(powerd/libpower_supply.pie.a): LDLIBS += $(POWERSUPPLY_LIBS)
clean: CLEAN(powerd/libpower_supply.pie.a)

LIBBACKLIGHTCTRL_OBJS = \
	powerd/backlight_controller.o \
	powerd/external_backlight_controller.o \
	powerd/internal_backlight_controller.o
CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a): \
	$(LIBBACKLIGHTCTRL_OBJS)
CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a): \
	CPPFLAGS += $(GLIB_FLAGS)
CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a): \
	LDLIBS += $(GLIB_LIBS)
clean: CLEAN(powerd/libbacklight_controller.pie.a)

powerd/powerd.o.depends: power_supply_properties.pb.h
powerd/powerd.o.depends: video_activity_update.pb.h
powerd/state_control.o.depends: power_supply_properties.pb.h
LIBPOWERD_DEPS = libchromeos-$(BASE_VER) libcras
LIBPOWERD_FLAGS = \
	$(GLIB_FLAGS) $(DBUS_FLAGS) \
	$(shell $(PKG_CONFIG) --cflags $(LIBPOWERD_DEPS))
LIBPOWERD_LIBS = \
	$(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lmetrics -ludev -lprotobuf-lite \
	$(shell $(PKG_CONFIG) --libs $(LIBPOWERD_DEPS))
LIBPOWERD_OBJS = \
	powerd/ambient_light_sensor.o \
	powerd/async_file_reader.o \
	powerd/file_tagger.o \
	powerd/idle_detector.o \
	powerd/keyboard_backlight_controller.o \
	powerd/metrics_constants.o \
	powerd/metrics_store.o \
	powerd/monitor_reconfigure.o \
	powerd/powerd_metrics.o \
	powerd/powerd.o \
	powerd/power_supply.o \
	powerd/rolling_average.o \
	powerd/screen_locker.o \
	powerd/state_control.o \
	powerd/suspend_delay_controller.o \
	powerd/suspender.o \
	powerd/video_detector.o \
	power_manager/suspend.pb.o \
	power_state_control.pb.o \
	power_supply_properties.pb.o \
	video_activity_update.pb.o
CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a): $(LIBPOWERD_OBJS)
CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a): CPPFLAGS += $(LIBPOWERD_FLAGS)
CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a): LDLIBS += $(LIBPOWERD_LIBS)
clean: CLEAN(powerd/libpowerd.pie.a)

POWERD_FLAGS = $(LIBPOWERD_FLAGS)
POWERD_LIBS = $(LIBPOWERD_LIBS)
POWERD_OBJS = powerd/powerd_main.o
CXX_BINARY(powerd/powerd): $(POWERD_OBJS) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(powerd/powerd): CPPFLAGS += $(POWERD_FLAGS)
CXX_BINARY(powerd/powerd): LDLIBS += $(POWERD_LIBS)
clean: CXX_BINARY(powerd/powerd)
all: CXX_BINARY(powerd/powerd)

POWERD_UNITTEST_FLAGS = $(POWERD_FLAGS)
POWERD_UNITTEST_LIBS = $(POWERD_LIBS) -lgtest -lgmock
POWERD_UNITTEST_OBJS = \
	powerd/ambient_light_sensor_unittest.o \
	powerd/async_file_reader_unittest.o \
	powerd/external_backlight_controller_unittest.o \
	powerd/file_tagger_unittest.o \
	powerd/idle_dimmer_unittest.o \
	powerd/internal_backlight_controller_unittest.o \
	powerd/keyboard_backlight_controller_unittest.o \
	powerd/metrics_store_unittest.o \
	powerd/monitor_reconfigure.o \
	powerd/plug_dimmer_unittest.o \
	powerd/powerd_unittest.o \
	powerd/power_supply_unittest.o \
	powerd/rolling_average_unittest.o \
	powerd/state_control_unittest.o \
	powerd/suspend_delay_controller_unittest.o \
	powerd/video_detector_unittest.o
CXX_BINARY(powerd/powerd_unittest): $(POWERD_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_test.pie.a)
CXX_BINARY(powerd/powerd_unittest): CPPFLAGS += $(POWERD_UNITTEST_FLAGS)
CXX_BINARY(powerd/powerd_unittest): LDLIBS += $(POWERD_UNITTEST_LIBS)
clean: CXX_BINARY(powerd/powerd_unittest)
tests: TEST(CXX_BINARY(powerd/powerd_unittest))
