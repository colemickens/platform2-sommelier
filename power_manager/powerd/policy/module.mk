# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_STATIC_LIBRARY(powerd/libpolicy.pie.a): \
	power_manager/input_event.pb.o \
	powerd/policy/input_controller.o
clean: CLEAN(powerd/libpolicy.pie.a)
