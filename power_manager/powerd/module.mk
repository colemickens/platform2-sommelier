# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

powerd/powerd.o.depends: power_supply_properties.pb.h
powerd/powerd.o.depends: video_activity_update.pb.h
LIBPOWERD_DEPS = libchromeos-$(BASE_VER) libcras
LIBPOWERD_FLAGS = \
	$(GLIB_FLAGS) $(DBUS_FLAGS) \
	$(shell $(PKG_CONFIG) --cflags $(LIBPOWERD_DEPS))
LIBPOWERD_LIBS = \
	$(GLIB_LIBS) $(DBUS_LIBS) -lgflags -lmetrics -ludev -lprotobuf-lite \
	-lrt $(shell $(PKG_CONFIG) --libs $(LIBPOWERD_DEPS))
LIBPOWERD_OBJS = \
	powerd/file_tagger.o \
	powerd/metrics_constants.o \
	powerd/metrics_store.o \
	powerd/powerd_metrics.o \
	powerd/powerd.o \
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
	CXX_STATIC_LIBRARY(common/libprefs.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpolicy.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(powerd/powerd): CPPFLAGS += $(POWERD_FLAGS)
CXX_BINARY(powerd/powerd): LDLIBS += $(POWERD_LIBS)
clean: CXX_BINARY(powerd/powerd)
all: CXX_BINARY(powerd/powerd)

CXX_BINARY(powerd/powerd_setuid_helper): powerd/powerd_setuid_helper.o
CXX_BINARY(powerd/powerd_setuid_helper): CPPFLAGS += \
	$(shell $(PKG_CONFIG) --cflags libchrome-$(BASE_VER))
CXX_BINARY(powerd/powerd_setuid_helper): LDLIBS += \
	-lgflags $(shell $(PKG_CONFIG) --libs libchrome-$(BASE_VER))
clean: CLEAN(powerd/powerd_setuid_helper)
all: CXX_BINARY(powerd/powerd_setuid_helper)

POWERD_UNITTEST_FLAGS = $(POWERD_FLAGS)
TEST_LIBS := $(shell gmock-config --libs) $(shell gtest-config --libs)
POWERD_UNITTEST_LIBS = $(POWERD_LIBS) $(TEST_LIBS)
POWERD_UNITTEST_OBJS = \
	powerd/file_tagger_unittest.o \
	powerd/metrics_store_unittest.o \
	powerd/powerd_unittest.o
CXX_BINARY(powerd/powerd_unittest): $(POWERD_UNITTEST_OBJS) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(common/libprefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_test.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpolicy.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem_test.pie.a)
CXX_BINARY(powerd/powerd_unittest): CPPFLAGS += $(POWERD_UNITTEST_FLAGS)
CXX_BINARY(powerd/powerd_unittest): LDLIBS += $(POWERD_UNITTEST_LIBS)
clean: CXX_BINARY(powerd/powerd_unittest)
tests: TEST(CXX_BINARY(powerd/powerd_unittest))
