# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

POWERSUPPLY_FLAGS = $(GLIB_FLAGS)
POWERSUPPLY_LIBS = $(GLIB_LIBS) -lgflags
LIBPOWER_SUPPLY_OBJS=common/inotify.o \
                     common/power_constants.o \
                     common/power_prefs.o \
                     powerd/power_supply.o
CXX_STATIC_LIBRARY(powerd/libpower_supply.pie.a): $(LIBPOWER_SUPPLY_OBJS)
CXX_STATIC_LIBRARY(powerd/libpower_supply.pie.a): \
	CPPFLAGS += $(POWERSUPPLY_FLAGS)
CXX_STATIC_LIBRARY(powerd/libpower_supply.pie.a): LDLIBS += $(POWERSUPPLY_LIBS)
clean: CLEAN(powerd/libpower_supply.pie.a)

BACKLIGHT_FLAGS = $(GLIB_FLAGS) $(DBUS_FLAGS)
BACKLIGHT_LIBS = $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lmetrics -ludev
LIBBACKLIGHT_OBJS = powerd/backlight.o common/power_constants.o
CXX_STATIC_LIBRARY(powerd/libbacklight.pie.a): $(LIBBACKLIGHT_OBJS)
CXX_STATIC_LIBRARY(powerd/libbacklight.pie.a): CPPFLAGS += $(BACKLIGHT_FLAGS)
CXX_STATIC_LIBRARY(powerd/libbacklight.pie.a): LDLIBS += $(BACKLIGHT_LIBS)
clean: CLEAN(powerd/libbacklight.pie.a)

LIBBACKLIGHTCTRL_FLAGS = $(GLIB_FLAGS)
LIBBACKLIGHTCTRL_LIBS = $(GLIB_LIBS)
ifeq ($(USE_IS_DESKTOP),)
LIBBACKLIGHTCTRL_OBJS = powerd/internal_backlight_controller.o
else
LIBBACKLIGHTCTRL_OBJS = powerd/external_backlight_controller.o
endif
CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a): \
	$(LIBBACKLIGHTCTRL_OBJS)
CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a): \
	CPPFLAGS += $(LIBBACKLIGHTCTRL_FLAGS)
CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a): \
	LDLIBS += $(LIBBACKLIGHTCTRL_LIBS)
clean: CLEAN(powerd/libbacklight_controller.pie.a)

powerd/powerd.o.depends: power_supply_properties.pb.h
powerd/powerd.o.depends: video_activity_update.pb.h
powerd/state_control.o.depends: power_supply_properties.pb.h
LIBPOWERD_DEPS = libchromeos-$(BASE_VER) libcras
LIBPOWERD_FLAGS = $(GLIB_FLAGS) $(DBUS_FLAGS) \
                  $(shell $(PKG_CONFIG) --cflags $(LIBPOWERD_DEPS))
LIBPOWERD_LIBS = $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lmetrics -ludev \
                 -lprotobuf-lite \
                 $(shell $(PKG_CONFIG) --libs $(LIBPOWERD_DEPS))
LIBPOWERD_OBJS = power_state_control.pb.o \
                 power_supply_properties.pb.o \
                 powerd/ambient_light_sensor.o \
                 powerd/async_file_reader.o \
                 powerd/audio_detector.o \
                 powerd/external_backlight_client.o \
                 powerd/file_tagger.o \
                 powerd/idle_detector.o \
                 powerd/keyboard_backlight_controller.o \
                 powerd/metrics_constants.o \
                 powerd/metrics_store.o \
                 powerd/monitor_reconfigure.o \
                 powerd/power_supply.o \
                 powerd/powerd.o \
                 powerd/powerd_metrics.o \
                 powerd/rolling_average.o \
                 powerd/screen_locker.o \
                 powerd/state_control.o \
                 powerd/suspender.o \
                 video_activity_update.pb.o \
                 powerd/video_detector.o
CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a): $(LIBPOWERD_OBJS)
CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a): CPPFLAGS += $(LIBPOWERD_FLAGS)
CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a): LDLIBS += $(LIBPOWERD_LIBS)
clean: CLEAN(powerd/libpowerd.pie.a)

POWERD_FLAGS = $(LIBPOWERD_FLAGS)
POWERD_LIBS = $(LIBPOWERD_LIBS)
POWERD_OBJS = powerd/powerd_main.o
CXX_BINARY(powerd/powerd): $(POWERD_OBJS) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(powerd/powerd): CPPFLAGS += $(POWERD_FLAGS)
CXX_BINARY(powerd/powerd): LDLIBS += $(POWERD_LIBS)
clean: CXX_BINARY(powerd/powerd)
all: CXX_BINARY(powerd/powerd)

FILE_TAGGER_UNITTEST_FLAGS = $(POWERD_FLAGS)
FILE_TAGGER_UNITTEST_LIBS = $(POWERD_LIBS) -lgtest -lgmock
FILE_TAGGER_UNITTEST_OBJS = powerd/file_tagger_unittest.o
CXX_BINARY(powerd/file_tagger_unittest): $(FILE_TAGGER_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(powerd/file_tagger_unittest): \
	CPPFLAGS += $(FILE_TAGGER_UNITTEST_FLAGS)
CXX_BINARY(powerd/file_tagger_unittest): \
	LDLIBS += $(FILE_TAGGER_UNITTEST_LIBS)
clean: CXX_BINARY(powerd/file_tagger_unittest)
tests: TEST(CXX_BINARY(powerd/file_tagger_unittest))

POWERD_UNITTEST_FLAGS = $(POWERD_FLAGS)
POWERD_UNITTEST_LIBS = $(POWERD_LIBS) -lgtest -lgmock
POWERD_UNITTEST_OBJS = powerd/metrics_store_unittest.o \
                       powerd/monitor_reconfigure.o \
                       powerd/powerd_unittest.o \
                       powerd/rolling_average_unittest.o \
                       powerd/video_detector_unittest.o
ifeq ($(USE_IS_DESKTOP),)
POWERD_UNITTEST_OBJS += powerd/internal_backlight_controller_unittest.o \
                        powerd/idle_dimmer_unittest.o \
                        powerd/plug_dimmer_unittest.o
else
POWERD_UNITTEST_OBJS += powerd/external_backlight_controller_unittest.o
endif

ifneq ($(USE_HAS_KEYBOARD_BACKLIGHT),)
POWERD_UNITTEST_OBJS += powerd/keyboard_backlight_controller_unittest.o
endif

CXX_BINARY(powerd/powerd_unittest): $(POWERD_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(powerd/powerd_unittest): CPPFLAGS += $(POWERD_UNITTEST_FLAGS)
CXX_BINARY(powerd/powerd_unittest): LDLIBS += $(POWERD_UNITTEST_LIBS)
clean: CXX_BINARY(powerd/powerd_unittest)
tests: TEST(CXX_BINARY(powerd/powerd_unittest))

BACKLIGHT_UNITTEST_FLAGS = $(GLIB_FLAGS)
BACKLIGHT_UNITTEST_LIBS = $(GLIB_LIBS) -lgmock -lgtest
BACKLIGHT_UNITTEST_OBJS = powerd/backlight_unittest.o
CXX_BINARY(powerd/backlight_unittest): $(BACKLIGHT_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight.pie.a)
CXX_BINARY(powerd/backlight_unittest): CPPFLAGS += $(BACKLIGHT_UNITTEST_FLAGS)
CXX_BINARY(powerd/backlight_unittest): LDLIBS += $(BACKLIGHT_UNITTEST_LIBS)
clean: CXX_BINARY(powerd/backlight_unittest)
tests: TEST(CXX_BINARY(powerd/backlight_unittest))

POWER_SUPPLY_UNITTEST_FLAGS = $(POWERD_FLAGS)
POWER_SUPPLY_UNITTEST_LIBS = $(POWERD_LIBS)  -lgtest -lgmock
POWER_SUPPLY_UNITTEST_OBJS = powerd/power_supply_unittest.o
CXX_BINARY(powerd/power_supply_unittest): $(POWER_SUPPLY_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(powerd/power_supply_unittest): \
	CPPFLAGS += $(POWER_SUPPLY_UNITTEST_FLAGS)
CXX_BINARY(powerd/power_supply_unittest): \
	LDLIBS += $(POWER_SUPPLY_UNITTEST_LIBS)
clean: CXX_BINARY(powerd/power_supply_unittest)
tests: TEST(CXX_BINARY(powerd/power_supply_unittest))

powerd/state_control_unittest.o.depends: power_state_control.pb.h
STATE_CONTROL_UNITTEST_FLAGS = $(POWERD_FLAGS)
STATE_CONTROL_UNITTEST_LIBS = $(POWERD_LIBS) -lgtest -lgmock
STATE_CONTROL_UNITTEST_OBJS = powerd/state_control_unittest.o \
                              powerd/state_control.o
CXX_BINARY(powerd/state_control_unittest): $(STATE_CONTROL_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(powerd/state_control_unittest): \
	CPPFLAGS += $(STATE_CONTROL_UNITTEST_FLAGS)
CXX_BINARY(powerd/state_control_unittest): \
	LDLIBS += $(STATE_CONTROL_UNITTEST_LIBS)
clean: CXX_BINARY(powerd/state_control_unittest)
tests: TEST(CXX_BINARY(powerd/state_control_unittest))

ASYNC_FILE_READER_UNITTEST_FLAGS = $(GLIB_FLAGS)
ASYNC_FILE_READER_UNITTEST_LIBS = $(GLIB_LIBS) -lgtest -lgmock -lrt
ASYNC_FILE_READER_UNITTEST_OBJS = powerd/async_file_reader_unittest.o \
                                  powerd/async_file_reader.o
CXX_BINARY(powerd/async_file_reader_unittest): \
	$(ASYNC_FILE_READER_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a)
CXX_BINARY(powerd/async_file_reader_unittest): CPPFLAGS += \
	$(ASYNC_FILE_READER_UNITTEST_FLAGS)
CXX_BINARY(powerd/async_file_reader_unittest): LDLIBS += \
	$(ASYNC_FILE_READER_UNITTEST_LIBS)
clean: CXX_BINARY(powerd/async_file_reader_unittest)
tests: TEST(CXX_BINARY(powerd/async_file_reader_unittest))
