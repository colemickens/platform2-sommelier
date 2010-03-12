// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>

#include "power_manager/xidle.h"

namespace power_manager {

class XIdleTest : public ::testing::Test { };

TEST(XIdleTest, GetIdleTimeTest) {
  int64 idle_time = kint64max;
  power_manager::XIdle idle;
  if (idle.Init()) {
    ASSERT_TRUE(idle.GetIdleTime(&idle_time));
    ASSERT_NE(kint64max, idle_time);
  }
}

}  // namespace power_manager
