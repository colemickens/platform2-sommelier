# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_STATIC_LIBRARY(powerd/libpolicy.pie.a): \
	power_manager/input_event.pb.o \
	power_manager/policy.pb.o \
	powerd/policy/ambient_light_handler.o \
	powerd/policy/dark_resume_policy.o \
	powerd/policy/external_backlight_controller.o \
	powerd/policy/input_controller.o \
	powerd/policy/internal_backlight_controller.o \
	powerd/policy/keyboard_backlight_controller.o \
	powerd/policy/state_controller.o \
	powerd/policy/suspend_delay_controller.o \
	powerd/policy/suspender.o \
	power_manager/suspend.pb.o
clean: CLEAN(powerd/libpolicy.pie.a)

CXX_STATIC_LIBRARY(powerd/libpolicy_test.pie.a): \
	powerd/policy/backlight_controller_stub.o
clean: CLEAN(powerd/libpolicy_test.pie.a)

CXX_BINARY(powerd/policy_unittest): \
	powerd/policy/ambient_light_handler_unittest.o \
	powerd/policy/dark_resume_policy_unittest.o \
	powerd/policy/external_backlight_controller_unittest.o \
	powerd/policy/input_controller_unittest.o \
	powerd/policy/internal_backlight_controller_unittest.o \
	powerd/policy/keyboard_backlight_controller_unittest.o \
	powerd/policy/state_controller_unittest.o \
	powerd/policy/suspend_delay_controller_unittest.o \
	powerd/policy/suspender_unittest.o \
	CXX_STATIC_LIBRARY(powerd/libpolicy.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpolicy_test.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libsystem_test.pie.a) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(common/libprefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_test.pie.a)
CXX_BINARY(powerd/policy_unittest): \
	CPPFLAGS += $(LIBCHROME_FLAGS)
CXX_BINARY(powerd/policy_unittest): \
	LDLIBS += $(LIBCHROME_LIBS) -lgtest -lprotobuf-lite -ludev
clean: CXX_BINARY(powerd/policy_unittest)
tests: TEST(CXX_BINARY(powerd/policy_unittest))
