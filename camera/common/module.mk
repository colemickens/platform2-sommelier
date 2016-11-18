# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

future_unittest_OBJS = common/future_unittest.o $(common_CXX_OBJECTS)
future_unittest_LIBS = -lgtest
CXX_BINARY(common/future_unittest): $(future_unittest_OBJS)
CXX_BINARY(common/future_unittest): LDLIBS += $(future_unittest_LIBS)
clean: CLEAN(common/future_unittest)
tests: CXX_BINARY(common/future_unittest)

# To link against object files under common/, add $(COMMON_OBJECTS) to the
# dependency list of your target.
COMMON_OBJECTS := common/future.o common/metadata_base.o
