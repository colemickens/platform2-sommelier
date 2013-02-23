# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_STATIC_LIBRARY(powerd/libpolicy.pie.a): \
	power_manager/input_event.pb.o \
	power_manager/policy.pb.o \
	powerd/policy/dark_resume_policy.o \
	powerd/policy/input_controller.o \
	powerd/policy/state_controller.o
clean: CLEAN(powerd/libpolicy.pie.a)

CXX_BINARY(powerd/policy_unittest): \
	powerd/policy/dark_resume_policy_unittest.o \
	powerd/policy/state_controller_unittest.o \
	CXX_STATIC_LIBRARY(powerd/libpolicy.pie.a) \
	CXX_STATIC_LIBRARY(powerd/libpowerd.pie.a) \
	CXX_STATIC_LIBRARY(common/libtestrunner.pie.a) \
	CXX_STATIC_LIBRARY(common/libprefs.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_test.pie.a)
CXX_BINARY(powerd/policy_unittest): \
	CPPFLAGS += $(GLIB_FLAGS)
CXX_BINARY(powerd/policy_unittest): \
	LDLIBS += $(GLIB_LIBS) -lgtest -lprotobuf-lite
clean: CXX_BINARY(powerd/policy_unittest)
tests: TEST(CXX_BINARY(powerd/policy_unittest))
