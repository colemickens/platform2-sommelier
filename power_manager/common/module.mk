# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

LIBPOWERPREFS_FLAGS = $(GLIB_FLAGS)
LIBPOWERPREFS_LIBS = $(GLIB_LIBS)
LIBPOWERPREFS_OBJS = common/power_prefs.o common/power_constants.o
CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a): $(LIBPOWERPREFS_OBJS)
CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a): \
	CPPFLAGS += $(LIBPOWERPREFS_FLAGS)
CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a): LDLIBS += $(LIBPOWERPREFS_LIBS)
clean: CLEAN(common/libpower_prefs.pie.a)

LIBUTIL_OBJS = common/util.o common/inotify.o
LIBUTIL_FLAGS = $(GLIB_FLAGS)
LIBUTIL_LIBS = $(GLIB_LIBS)
CXX_STATIC_LIBRARY(common/libutil.pie.a): $(LIBUTIL_OBJS)
CXX_STATIC_LIBRARY(common/libutil.pie.a): CPPFLAGS += $(LIBUTIL_FLAGS)
CXX_STATIC_LIBRARY(common/libutil.pie.a): LDLIBS += $(LIBUTIL_LIBS)
clean: CLEAN(common/libutil.pie.a)

LIBUTIL_DBUS_OBJS = common/util_dbus.o common/util_dbus_handler.o
LIBUTIL_DBUS_FLAGS = $(DBUS_FLAGS) $(GLIB_FLAGS)
LIBUTIL_DBUS_LIBS = $(DBUS_LIBS) $(GLIB_LIBS)
CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a): $(LIBUTIL_DBUS_OBJS)
CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a): CPPFLAGS += $(LIBUTIL_DBUS_FLAGS)
CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a): LDLIBS += $(LIBUTIL_DBUS_LIBS)
clean: CLEAN(common/libutil_dbus.pie.a)

LIBTESTRUNNER_OBJS = common/testrunner.o
CXX_STATIC_LIBRARY(common/libtestrunner.pie.a): $(LIBTESTRUNNER_OBJS)
clean: CLEAN(common/libtestrunner.pie.a)

UTIL_UNITTEST_FLAGS = $(LIBUTIL_FLAGS)
UTIL_UNITTEST_LIBS = $(LIBUTIL_LIBS) -lgtest -lgmock
UTIL_UNITTEST_OBJS = common/util_unittest.o common/util.o
CXX_BINARY(common/util_unittest): $(UTIL_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a)
CXX_BINARY(common/util_unittest): CPPFLAGS +=$(UTIL_UNITTEST_FLAGS)
CXX_BINARY(common/util_unittest): LDLIBS += $(UTIL_UNITTEST_LIBS)
clean: CXX_BINARY(common/util_unittest)
tests: TEST(CXX_BINARY(common/util_unittest))

POWER_PREFS_UNITTEST_FLAGS = $(GLIB_FLAGS)
POWER_PREFS_UNITTEST_LIBS = $(GLIB_LIBS) -lgtest -lgmock
POWER_PREFS_UNITTEST_OBJS = common/inotify.o \
                            common/power_prefs_unittest.o \
                            common/power_prefs.o
CXX_BINARY(common/power_prefs_unittest): $(POWER_PREFS_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a)
CXX_BINARY(common/power_prefs_unittest): \
	CPPFLAGS +=$(POWER_PREFS_UNITTEST_FLAGS)
CXX_BINARY(common/power_prefs_unittest): LDLIBS += $(POWER_PREFS_UNITTEST_LIBS)
clean: CXX_BINARY(common/power_prefs_unittest)
tests: TEST(CXX_BINARY(common/power_prefs_unittest))
