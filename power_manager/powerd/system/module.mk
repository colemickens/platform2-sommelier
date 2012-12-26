# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_STATIC_LIBRARY(powerd/libsystem.pie.a): \
	powerd/system/external_backlight.o \
	powerd/system/input.o \
	powerd/system/internal_backlight.o
CXX_STATIC_LIBRARY(powerd/libsystem.pie.a): \
	CPPFLAGS += $(GLIB_FLAGS)
CXX_STATIC_LIBRARY(powerd/libsystem.pie.a): \
	LDLIBS += $(GLIB_LIBS) -ludev
clean: CLEAN(powerd/libsystem.pie.a)

CXX_BINARY(powerd/system_unittest): \
	powerd/system/input_unittest.o \
	powerd/system/internal_backlight_unittest.o \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a)
CXX_BINARY(powerd/system_unittest): \
	CPPFLAGS += $(GLIB_FLAGS)
CXX_BINARY(powerd/system_unittest): \
	LDLIBS += $(GLIB_LIBS) -lgtest -lgmock -ludev
clean: CXX_BINARY(powerd/system_unittest)
tests: TEST(CXX_BINARY(powerd/system_unittest))
