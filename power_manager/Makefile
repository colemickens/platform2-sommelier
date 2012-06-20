# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Makefile uses ChromeOS's common.mk for most of its functionality. Consult
# the documentation in common-mk for recipes and other information.

# This controls where we build the package. Don't fiddle with this, since things
# like CPPFLAGS and some of the protobuf stuff depends on it.
OUT ?= $(PWD)/out
VERBOSE ?= 0

include common.mk

PROTOC ?= protoc
# BASE_VER is used to determine what versions of libbase and libchromeos we are
# going to compile against
BASE_VER ?= 125070
PKG_CONFIG ?= pkg-config

# base_env
CPPFLAGS += -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -I../../ -I.
CPPFLAGS-$(USE_NEW_POWER_BUTTON) += -DNEW_POWER_BUTTON
CPPFLAGS-$(USE_LOCKVT) += -DSUSPEND_LOCK_VT
CPPFLAGS-$(USE_IS_DESKTOP) += -DIS_DESKTOP
CPPFLAGS-$(USE_ALS) += -DHAS_ALS
CPPFLAGS-$(USE_HAS_KEYBOARD_BACKLIGHT) += -DHAS_KEYBOARD_BACKLIGHT
CPPFLAGS-$(USE_AURA) += -DUSE_AURA
CPPFLAGS += $(CPPFLAGS-y)

## protobuffer targets
# Detailed instructions on how to work with these rules can be found in the
# common-mk package.  These rules are for including protobufs that live in the
# dbus directory of the system_api repo. To add a new one from this location to
# the build, just add the name of the generated .cc file to the bindings
# variable and include approriate depends targets[.
#
# To add protobufs from a different location then you need to copy all the proto
# variables and use a new prefix for them, setting the path variable as you
# need.
#
# TODO(rharrison:crosbug.com/30056): Once I have written the protobuf macros,
#                                    use them here
SYSTEM_API_PROTO_BINDINGS = power_state_control.pb.cc \
                            power_supply_properties.pb.cc
SYSTEM_API_PROTO_PATH = $(SYSROOT)/usr/include/chromeos/dbus
SYSTEM_API_PROTO_HEADERS = $(patsubst %.cc,%.h,$(SYSTEM_API_PROTO_BINDINGS))
SYSTEM_API_PROTO_OBJS = $(patsubst %.cc,%.o,$(SYSTEM_API_PROTO_BINDINGS))
$(SYSTEM_API_PROTO_HEADERS): %.h: %.cc ;
$(SYSTEM_API_PROTO_BINDINGS): %.pb.cc: $(SYSTEM_API_PROTO_PATH)/%.proto
	$(PROTOC) --proto_path=$(SYSTEM_API_PROTO_PATH) --cpp_out=. $<
clean: CLEAN($(SYSTEM_API_PROTO_BINDINGS))
clean: CLEAN($(SYSTEM_API_PROTO_HEADERS))
clean: CLEAN($(SYSTEM_API_PROTO_OBJS))
# Add rules for compiling generated protobuffer code, as the CXX_OBJECTS list
# is built before these source files exists and, as such, does not contain them.
$(eval $(call add_object_rules,$(SYSTEM_API_PROTO_OBJS),CXX,cc))

## binary and library targets

DBUS_DEPS = dbus-1 dbus-glib-1
DBUS_FLAGS := $(shell $(PKG_CONFIG) --cflags $(DBUS_DEPS))
DBUS_LIBS := $(shell $(PKG_CONFIG) --libs $(DBUS_DEPS))

GLIB_DEPS = glib-2.0 gobject-2.0 libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
GLIB_FLAGS := $(shell $(PKG_CONFIG) --cflags $(GLIB_DEPS))
GLIB_LIBS := $(shell $(PKG_CONFIG) --libs $(GLIB_DEPS))

BACKLIGHT_FLAGS = $(GLIB_FLAGS) $(DBUS_FLAGS)
BACKLIGHT_LIBS = $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lmetrics -lX11 -lXrandr \
                 -ludev
LIBBACKLIGHT_OBJS = ambient_light_sensor.o async_file_reader.o backlight.o \
                    external_backlight.o external_backlight_client.o \
                    power_constants.o
CXX_STATIC_LIBRARY(libbacklight.pie.a): $(LIBBACKLIGHT_OBJS)
CXX_STATIC_LIBRARY(libbacklight.pie.a): CPPFLAGS += $(BACKLIGHT_FLAGS)
CXX_STATIC_LIBRARY(libbacklight.pie.a): LDLIBS += $(BACKLIGHT_LIBS)
clean: CLEAN(libbacklight.pie.a)

BACKLIGHT-TOOL_OBJS = backlight_tool.o
CXX_BINARY(backlight-tool): $(BACKLIGHT-TOOL_OBJS) \
	CXX_STATIC_LIBRARY(libbacklight.pie.a)
CXX_BINARY(backlight-tool): CPPFLAGS += $(BACKLIGHT_FLAGS)
CXX_BINARY(backlight-tool): LDLIBS += $(BACKLIGHT_LIBS)
clean: CLEAN(backlight-tool)
all: CXX_BINARY(backlight-tool)

POWERSUPPLY_FLAGS = $(GLIB_FLAGS)
POWERSUPPLY_LIBS = $(GLIB_LIBS) -lgflags

LIBPOWER_SUPPLY_OBJS=power_supply.o power_prefs.o inotify.o
CXX_STATIC_LIBRARY(libpower_supply.pie.a): $(LIBPOWER_SUPPLY_OBJS)
CXX_STATIC_LIBRARY(libpower_supply.pie.a): CPPFLAGS += $(POWERSUPPLY_FLAGS)
CXX_STATIC_LIBRARY(libpower_supply.pie.a): LDLIBS += $(POWERSUPPLY_LIBS)
clean: CLEAN(libpower_supply.pie.a)

POWER-SUPPLY-INFO_OBJS = power_supply_info.o
CXX_BINARY(power-supply-info): $(POWER-SUPPLY-INFO_OBJS) \
	CXX_STATIC_LIBRARY(libpower_supply.pie.a)
CXX_BINARY(power-supply-info): CPPFLAGS += $(POWERSUPPLY_FLAGS)
CXX_BINARY(power-supply-info): LDLIBS += $(POWERSUPPLY_LIBS)
clean: CLEAN(power-supply-info)
all: CXX_BINARY(power-supply-info)

LIBBACKLIGHTCTRL_FLAGS = $(GLIB_FLAGS)
LIBBACKLIGHTCTRL_LIBS = $(GLIB_LIBS)

ifeq ($(USE_IS_DESKTOP),)
LIBBACKLIGHTCTRL_OBJS = internal_backlight_controller.o
else
LIBBACKLIGHTCTRL_OBJS = external_backlight_controller.o
endif
CXX_STATIC_LIBRARY(libbacklight_controller.pie.a): $(LIBBACKLIGHTCTRL_OBJS)
CXX_STATIC_LIBRARY(libbacklight_controller.pie.a): CPPFLAGS += \
                                                   $(LIBBACKLIGHTCTRL_FLAGS)
CXX_STATIC_LIBRARY(libbacklight_controller.pie.a): LDLIBS += \
                                                   $(LIBBACKLIGHTCTRL_LIBS)
clean: CLEAN(libbacklight_controller.pie.a)

LIBPOWERPREFS_FLAGS = $(GLIB_FLAGS)
LIBPOWERPREFS_LIBS = $(GLIB_LIBS)
LIBPOWERPREFS_OBJS = power_prefs.o power_constants.o
CXX_STATIC_LIBRARY(libpower_prefs.pie.a): $(LIBPOWERPREFS_OBJS)
CXX_STATIC_LIBRARY(libpower_prefs.pie.a): CPPFLAGS += $(LIBPOWERPREFS_FLAGS)
CXX_STATIC_LIBRARY(libpower_prefs.pie.a): LDLIBS += $(LIBPOWERPREFS_LIBS)
clean: CLEAN(libpower_prefs.pie.a)

LIBPOWERD_DEPS = libchromeos-$(BASE_VER)
LIBPOWERD_FLAGS := $(GLIB_FLAGS) $(DBUS_FLAGS) \
                   $(shell $(PKG_CONFIG) --cflags $(LIBPOWERD_DEPS))
LIBPOWERD_LIBS := $(GLIB_LIBS) $(DBUS_LIBS) -lXext -lgflags -lmetrics -lX11 \
                  -lXrandr -ludev -lprotobuf-lite \
                  $(shell $(PKG_CONFIG) --libs $(LIBPOWERD_DEPS))

LIBUTIL_OBJS = util.o inotify.o
LIBUTIL_FLAGS = $(GLIB_FLAGS)
LIBUTIL_LIBS = $(GLIB_LIBS)
CXX_STATIC_LIBRARY(libutil.pie.a): $(LIBUTIL_OBJS)
CXX_STATIC_LIBRARY(libutil.pie.a): CPPFLAGS += $(LIBUTIL_FLAGS)
CXX_STATIC_LIBRARY(libutil.pie.a): LDLIBS += $(LIBUTIL_LIBS)
clean: CLEAN(libutil.pie.a)

LIBUTIL_DBUS_OBJS = util_dbus.o
LIBUTIL_DBUS_FLAGS = $(DBUS_FLAGS) $(GLIB_FLAGS)
LIBUTIL_DBUS_LIBS = $(DBUS_LIBS) $(GLIB_LIBS)
CXX_STATIC_LIBRARY(libutil_dbus.pie.a): $(LIBUTIL_DBUS_OBJS)
CXX_STATIC_LIBRARY(libutil_dbus.pie.a): CPPFLAGS += $(LIBUTIL_DBUS_FLAGS)
CXX_STATIC_LIBRARY(libutil_dbus.pie.a): LDLIBS += $(LIBUTIL_DBUS_LIBS)
clean: CLEAN(libutil_dbus.pie.a)

powerd.o.depends: power_supply_properties.pb.h
state_control.o.depends: power_supply_properties.pb.h
LIBPOWERD_OBJS = power_state_control.pb.o power_supply_properties.pb.o \
                 async_file_reader.o audio_detector.o file_tagger.o \
                 idle_detector.o metrics_constants.o metrics_store.o \
                 monitor_reconfigure.o power_supply.o powerd.o \
                 powerd_metrics.o rolling_average.o \
                 screen_locker.o state_control.o suspender.o video_detector.o
CXX_STATIC_LIBRARY(libpowerd.pie.a): $(LIBPOWERD_OBJS)
CXX_STATIC_LIBRARY(libpowerd.pie.a): CPPFLAGS += $(LIBPOWERD_FLAGS)
CXX_STATIC_LIBRARY(libpowerd.pie.a): LDLIBS += $(LIBPOWERD_LIBS)
clean: CLEAN(libpowerd.pie.a)

POWERD_FLAGS = $(LIBPOWERD_FLAGS)
POWERD_LIBS = $(LIBPOWERD_LIBS)
POWERD_OBJS = powerd_main.o
CXX_BINARY(powerd): $(POWERD_OBJS) \
	CXX_STATIC_LIBRARY(libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(libutil.pie.a) \
	CXX_STATIC_LIBRARY(libutil_dbus.pie.a)
CXX_BINARY(powerd): CPPFLAGS += $(POWERD_FLAGS)
CXX_BINARY(powerd): LDLIBS += $(POWERD_LIBS)
clean: CXX_BINARY(powerd)
all: CXX_BINARY(powerd)

LIBPOWERMAN_DEPS = libchromeos-$(BASE_VER)
LIBPOWERMAN_FLAGS := $(GLIB_FLAGS) $(DBUS_FLAGS) \
                     $(shell $(PKG_CONFIG) --cflags $(LIBPOWERMAN_DEPS))
LIBPOWERMAN_LIBS := $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lmetrics -ludev -lX11 \
                    $(shell $(PKG_CONFIG) --libs $(LIBPOWERMAN_DEPS))
LIBPOWERMAN_OBJS = powerman.o external_backlight.o input.o powerman_metrics.o
CXX_STATIC_LIBRARY(libpowerman.pie.a): $(LIBPOWERMAN_OBJS)
CXX_STATIC_LIBRARY(libpowerman.pie.a): CPPFLAGS += $(LIBPOWERMAN_FLAGS)
CXX_STATIC_LIBRARY(libpowerman.pie.a): LDLIBS += $(LIBPOWERMAN_LIBS)
clean: CLEAN(libpowerman.pie.a)

POWERMAN_FLAGS = $(LIBPOWERMAN_FLAGS)
POWERMAN_LIBS = $(LIBPOWERMAN_LIBS)
POWERMAN_OBJS = powerman_main.o
CXX_BINARY(powerm): $(POWERMAN_OBJS) \
	CXX_STATIC_LIBRARY(libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(libpowerman.pie.a) \
	CXX_STATIC_LIBRARY(libutil.pie.a) \
	CXX_STATIC_LIBRARY(libutil_dbus.pie.a)
CXX_BINARY(powerm): CPPFLAGS += $(POWERMAN_FLAGS)
CXX_BINARY(powerm): LDLIBS += $(POWERMAN_LIBS)
clean: CXX_BINARY(powerm)
all: CXX_BINARY(powerm)

SUSPENDDELAYSAMPLE_FLAGS = $(LIBPOWERMAN_FLAGS)
SUSPENDDELAYSAMPLE_LIBS = $(LIBPOWERMAN_LIBS) -lgflags
SUSPENDDELAYSAMPLE_OBJS = suspend_delay_sample.o
CXX_BINARY(suspend_delay_sample): $(SUSPENDDELAYSAMPLE_OBJS)
CXX_BINARY(suspend_delay_sample): CPPFLAGS += $(SUSPENDDELAYSAMPLE_FLAGS)
CXX_BINARY(suspend_delay_sample): LDLIBS += $(SUSPENDDELAYSAMPLE_LIBS)
clean: CXX_BINARY(suspend_delay_sample)
all: CXX_BINARY(suspend_delay_sample)

power_state_tool.o.depends: power_state_control.pb.h
POWERSTATETOOL_DEPS = libchromeos-$(BASE_VER)
POWERSTATETOOL_FLAGS := $(GLIB_FLAGS) $(DBUS_FLAGS) \
                        $(shell $(PKG_CONFIG) --cflags $(POWERSTATETOOL_DEPS))
POWERSTATETOOL_LIBS := $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lprotobuf-lite \
                       -lX11 \
                       $(shell $(PKG_CONFIG) --libs $(POWERSTATETOOL_DEPS))
POWERSTATETOOL_OBJS = power_state_control.pb.o power_state_tool.o \
                      power_constants.o
CXX_BINARY(power_state_tool): $(POWERSTATETOOL_OBJS) \
	CXX_STATIC_LIBRARY(libutil_dbus.pie.a)
CXX_BINARY(power_state_tool): CPPFLAGS += $(POWERSTATETOOL_FLAGS)
CXX_BINARY(power_state_tool): LDLIBS += $(POWERSTATETOOL_LIBS)
clean: CXX_BINARY(power_state_tool)
all: CXX_BINARY(power_state_tool)

BACKLIGHTDBUSTOOL_DEPS = libchromeos-$(BASE_VER)
BACKLIGHTDBUSTOOL_FLAGS := $(GLIB_FLAGS) $(DBUS_FLAGS) \
	$(shell $(PKG_CONFIG) --cflags $(POWERSTATETOOL_DEPS))
BACKLIGHTDBUSTOOL_LIBS := $(GLIB_LIBS) $(DBUS_LIBS) -lrt -lpthread -lgflags \
                          -lprotobuf-lite -lX11 \
                          $(shell $(PKG_CONFIG) --libs $(POWERSTATETOOL_DEPS))
BACKLIGHTDBUSTOOL_OBJS = backlight_dbus_tool.o power_constants.o
CXX_BINARY(backlight_dbus_tool): $(BACKLIGHTDBUSTOOL_OBJS) \
	CXX_STATIC_LIBRARY(libutil_dbus.pie.a)
CXX_BINARY(backlight_dbus_tool): CPPFLAGS += $(BACKLIGHTDBUSTOOL_FLAGS)
CXX_BINARY(backlight_dbus_tool): LDLIBS += $(BACKLIGHTDBUSTOOL_LIBS)
clean: CXX_BINARY(backlight_dbus_tool)
all: CXX_BINARY(backlight_dbus_tool)

## Unit tests
LIBTESTRUNNER_OBJS = testrunner.o
CXX_STATIC_LIBRARY(libtestrunner.pie.a): $(LIBTESTRUNNER_OBJS)
clean: CLEAN(libtestrunner.pie.a)

BACKLIGHT_UNITTEST_FLAGS = $(GLIB_FLAGS)
BACKLIGHT_UNITTEST_LIBS = $(GLIB_LIBS) -lgmock -lgtest -lXext
BACKLIGHT_UNITTEST_OBJS = backlight_unittest.o
CXX_BINARY(backlight_unittest): $(BACKLIGHT_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(libutil.pie.a)
CXX_BINARY(backlight_unittest): CPPFLAGS += $(BACKLIGHT_UNITTEST_FLAGS)
CXX_BINARY(backlight_unittest): LDLIBS += $(BACKLIGHT_UNITTEST_LIBS)
clean: CXX_BINARY(backlight_unittest)
tests: TEST(CXX_BINARY(backlight_unittest))

POWERD_UNITTEST_FLAGS = $(POWERD_FLAGS)
POWERD_UNITTEST_LIBS = $(POWERD_LIBS) -lgtest -lgmock -lX11 -lXext -lXrandr \
                       -lXtst
POWERD_UNITTEST_OBJS = powerd_unittest.o metrics_store_unittest.o \
                       rolling_average_unittest.o
ifeq ($(USE_IS_DESKTOP),)
POWERD_UNITTEST_OBJS += internal_backlight_controller_unittest.o \
                        idle_dimmer_unittest.o monitor_reconfigure.o \
                        plug_dimmer_unittest.o
else
POWERD_UNITTEST_OBJS += external_backlight_controller_unittest.o \
                        monitor_reconfigure.o
endif

CXX_BINARY(powerd_unittest): $(POWERD_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(libutil.pie.a) \
	CXX_STATIC_LIBRARY(libutil_dbus.pie.a)
CXX_BINARY(powerd_unittest): CPPFLAGS += $(POWERD_UNITTEST_FLAGS)
CXX_BINARY(powerd_unittest): LDLIBS += $(POWERD_UNITTEST_LIBS)
clean: CXX_BINARY(powerd_unittest)
tests: TEST(CXX_BINARY(powerd_unittest))

FILE_TAGGER_UNITTEST_FLAGS = $(POWERD_FLAGS)
FILE_TAGGER_UNITTEST_LIBS = $(POWERD_LIBS) -lgtest -lgmock -lXtst
FILE_TAGGER_UNITTEST_OBJS = file_tagger_unittest.o
CXX_BINARY(file_tagger_unittest): $(FILE_TAGGER_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(libutil.pie.a)
CXX_BINARY(file_tagger_unittest): CPPFLAGS += $(FILE_TAGGER_UNITTEST_FLAGS)
CXX_BINARY(file_tagger_unittest): LDLIBS += $(FILE_TAGGER_UNITTEST_LIBS)
clean: CXX_BINARY(file_tagger_unittest)
tests: TEST(CXX_BINARY(file_tagger_unittest))

POWER_SUPPLY_UNITTEST_FLAGS = $(POWERD_FLAGS)
POWER_SUPPLY_UNITTEST_LIBS = $(POWERD_LIBS)  -lgtest -lgmock -lXtst
POWER_SUPPLY_UNITTEST_OBJS = power_supply_unittest.o
CXX_BINARY(power_supply_unittest): $(POWER_SUPPLY_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(libutil.pie.a)
CXX_BINARY(power_supply_unittest): CPPFLAGS += $(POWER_SUPPLY_UNITTEST_FLAGS)
CXX_BINARY(power_supply_unittest): LDLIBS += $(POWER_SUPPLY_UNITTEST_LIBS)
clean: CXX_BINARY(power_supply_unittest)
tests: TEST(CXX_BINARY(power_supply_unittest))

POWERMAN_UNITTEST_FLAGS = $(POWERMAN_FLAGS)
POWERMAN_UNITTEST_LIBS = $(POWERMAN_LIBS) -lgtest -lgmock
POWERMAN_UNITTEST_OBJS = powerman_unittest.o
CXX_BINARY(powerman_unittest): $(POWERMAN_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(libpowerman.pie.a) \
	CXX_STATIC_LIBRARY(libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(libutil.pie.a) \
	CXX_STATIC_LIBRARY(libutil_dbus.pie.a)
CXX_BINARY(powerman_unittest): CPPFLAGS += $(POWERMAN_UNITTEST_FLAGS)
CXX_BINARY(powerman_unittest): LDLIBS += $(POWERMAN_UNITTEST_LIBS)
clean: CXX_BINARY(powerman_unittest)
tests: TEST(CXX_BINARY(powerman_unittest))

state_control_unittest.o.depends: power_state_control.pb.cc
STATE_CONTROL_UNITTEST_FLAGS = $(POWERD_FLAGS)
STATE_CONTROL_UNITTEST_LIBS = $(POWERD_LIBS) -lgtest -lgmock -lXtst
STATE_CONTROL_UNITTEST_OBJS = state_control_unittest.o state_control.o
CXX_BINARY(state_control_unittest): $(STATE_CONTROL_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight_controller.pie.a) \
	CXX_STATIC_LIBRARY(libbacklight.pie.a) \
	CXX_STATIC_LIBRARY(libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(libutil.pie.a) \
	CXX_STATIC_LIBRARY(libutil_dbus.pie.a)
CXX_BINARY(state_control_unittest): CPPFLAGS += $(STATE_CONTROL_UNITTEST_FLAGS)
CXX_BINARY(state_control_unittest): LDLIBS += $(STATE_CONTROL_UNITTEST_LIBS)
clean: CXX_BINARY(state_control_unittest)
tests: TEST(CXX_BINARY(state_control_unittest))

ASYNC_FILE_READER_UNITTEST_FLAGS = $(GLIB_FLAGS)
ASYNC_FILE_READER_UNITTEST_LIBS = $(GLIB_LIBS) -lgtest -lgmock -lXtst
ASYNC_FILE_READER_UNITTEST_OBJS = async_file_reader_unittest.o \
                                  async_file_reader.o
CXX_BINARY(async_file_reader_unittest): $(ASYNC_FILE_READER_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(libtestrunner.pie.a)
CXX_BINARY(async_file_reader_unittest): CPPFLAGS += \
	$(ASYNC_FILE_READER_UNITTEST_FLAGS)
CXX_BINARY(async_file_reader_unittest): LDLIBS += \
	$(ASYNC_FILE_READER_UNITTEST_LIBS)
clean: CXX_BINARY(async_file_reader_unittest)
tests: TEST(CXX_BINARY(async_file_reader_unittest))

UTIL_UNITTEST_FLAGS = $(LIBUTIL_FLAGS)
UTIL_UNITTEST_LIBS = $(LIBUTIL_LIBS) -lgtest -lgmock -lX11
UTIL_UNITTEST_OBJS = util_unittest.o util.o
CXX_BINARY(util_unittest): $(UTIL_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(libtestrunner.pie.a)
CXX_BINARY(util_unittest): CPPFLAGS +=$(UTIL_UNITTEST_FLAGS)
CXX_BINARY(util_unittest): LDLIBS += $(UTIL_UNITTEST_LIBS)
clean: CXX_BINARY(util_unittest)
tests: TEST(CXX_BINARY(util_unittest))
