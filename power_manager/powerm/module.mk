# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_STATIC_LIBRARY(powerm/libexternal_backlight.pie.a): \
	powerm/external_backlight.o common/power_constants.o
CXX_STATIC_LIBRARY(powerm/libexternal_backlight.pie.a): \
	CPPFLAGS += $(GLIB_FLAGS) $(DBUS_FLAGS)
CXX_STATIC_LIBRARY(powerm/libexternal_backlight.pie.a): \
	LDLIBS += $(GLIB_LIBS) $(DBUS_LIBS) -ludev
clean: CLEAN(powerm/libexternal_backlight.pie.a)

LIBPOWERMAN_DEPS = libchromeos-$(BASE_VER)
LIBPOWERMAN_FLAGS = $(GLIB_FLAGS) $(DBUS_FLAGS) \
                    $(shell $(PKG_CONFIG) --cflags $(LIBPOWERMAN_DEPS))
LIBPOWERMAN_LIBS = $(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lmetrics -ludev \
                   $(shell $(PKG_CONFIG) --libs $(LIBPOWERMAN_DEPS))
LIBPOWERMAN_OBJS = common/power_constants.o \
                   powerm/input.o \
                   powerm/powerman.o \
                   powerm/powerman_metrics.o
CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a): $(LIBPOWERMAN_OBJS)
CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a): CPPFLAGS += $(LIBPOWERMAN_FLAGS)
CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a): LDLIBS += $(LIBPOWERMAN_LIBS)
clean: CLEAN(powerm/libpowerman.pie.a)

POWERMAN_FLAGS = $(LIBPOWERMAN_FLAGS)
POWERMAN_LIBS = $(LIBPOWERMAN_LIBS)
POWERMAN_OBJS = powerm/powerman_main.o
CXX_BINARY(powerm/powerm): $(POWERMAN_OBJS) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a) \
	CXX_STATIC_LIBRARY(powerm/libexternal_backlight.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(powerm/powerm): CPPFLAGS += $(POWERMAN_FLAGS)
CXX_BINARY(powerm/powerm): LDLIBS += $(POWERMAN_LIBS)
clean: CXX_BINARY(powerm/powerm)
all: CXX_BINARY(powerm/powerm)

POWERMAN_UNITTEST_FLAGS = $(POWERMAN_FLAGS)
POWERMAN_UNITTEST_LIBS = $(POWERMAN_LIBS) -lgtest -lgmock
POWERMAN_UNITTEST_OBJS = powerm/powerman_unittest.o
CXX_BINARY(powerm/powerman_unittest): $(POWERMAN_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(powerm/powerman_unittest): CPPFLAGS += $(POWERMAN_UNITTEST_FLAGS)
CXX_BINARY(powerm/powerman_unittest): LDLIBS += $(POWERMAN_UNITTEST_LIBS)
clean: CXX_BINARY(powerm/powerman_unittest)
tests: TEST(CXX_BINARY(powerm/powerman_unittest))
