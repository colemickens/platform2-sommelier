# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

LIBPREFS_OBJS = \
	common/prefs.o \
	common/power_constants.o
LIBPREFS_FLAGS = $(LIBCHROME_FLAGS)
LIBPREFS_LIBS = $(LIBCHROME_LIBS)
CXX_STATIC_LIBRARY(common/libprefs.pie.a): $(LIBPREFS_OBJS)
CXX_STATIC_LIBRARY(common/libprefs.pie.a): \
	CPPFLAGS += $(LIBPREFS_FLAGS)
CXX_STATIC_LIBRARY(common/libprefs.pie.a): LDLIBS += $(LIBPREFS_LIBS)
clean: CLEAN(common/libprefs.pie.a)

LIBUTIL_OBJS = \
	common/clock.o \
	common/dbus_sender.o \
	common/power_constants.o \
	common/util.o
LIBUTIL_FLAGS = $(LIBCHROME_FLAGS)
LIBUTIL_LIBS = $(LIBCHROME_LIBS)
CXX_STATIC_LIBRARY(common/libutil.pie.a): $(LIBUTIL_OBJS)
CXX_STATIC_LIBRARY(common/libutil.pie.a): CPPFLAGS += $(LIBUTIL_FLAGS)
CXX_STATIC_LIBRARY(common/libutil.pie.a): LDLIBS += $(LIBUTIL_LIBS)
clean: CLEAN(common/libutil.pie.a)

LIBTESTRUNNER_OBJS = common/testrunner.o
CXX_STATIC_LIBRARY(common/libtestrunner.pie.a): $(LIBTESTRUNNER_OBJS)
clean: CLEAN(common/libtestrunner.pie.a)

LIBUTIL_TEST_OBJS = \
	common/dbus_sender_stub.o \
	common/fake_prefs.o \
	common/test_main_loop_runner.o \
	common/test_util.o
LIBUTIL_TEST_FLAGS = $(LIBCHROME_FLAGS)
LIBUTIL_TEST_LIBS = $(LIBCHROME_LIBS)
CXX_STATIC_LIBRARY(common/libutil_test.pie.a): $(LIBUTIL_TEST_OBJS)
CXX_STATIC_LIBRARY(common/libutil_test.pie.a): CPPFLAGS += $(LIBUTIL_TEST_FLAGS)
CXX_STATIC_LIBRARY(common/libutil_test.pie.a): LDLIBS += $(LIBUTIL_TEST_LIBS)
clean: CLEAN(common/libutil_test.pie.a)

UTIL_UNITTEST_OBJS = common/util_unittest.o
UTIL_UNITTEST_FLAGS = $(LIBUTIL_FLAGS)
UTIL_UNITTEST_LIBS = $(LIBUTIL_LIBS) -lgtest
CXX_BINARY(common/util_unittest): $(UTIL_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(common/util_unittest): CPPFLAGS +=$(UTIL_UNITTEST_FLAGS)
CXX_BINARY(common/util_unittest): LDLIBS += $(UTIL_UNITTEST_LIBS)
clean: CXX_BINARY(common/util_unittest)
tests: TEST(CXX_BINARY(common/util_unittest))

PREFS_UNITTEST_OBJS = common/prefs_unittest.o
PREFS_UNITTEST_FLAGS = $(LIBCHROME_FLAGS)
PREFS_UNITTEST_LIBS = $(LIBCHROME_LIBS) -lgtest
CXX_BINARY(common/prefs_unittest): $(PREFS_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_test.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libprefs.pie.a)
CXX_BINARY(common/prefs_unittest): \
	CPPFLAGS +=$(PREFS_UNITTEST_FLAGS)
CXX_BINARY(common/prefs_unittest): LDLIBS += $(PREFS_UNITTEST_LIBS)
clean: CXX_BINARY(common/prefs_unittest)
tests: TEST(CXX_BINARY(common/prefs_unittest))
