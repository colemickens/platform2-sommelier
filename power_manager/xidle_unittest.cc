// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>

#include "power_manager/xidle.h"

namespace chromeos {

class XIdleTest : public ::testing::Test { };

TEST(XIdleTest, GetIdleTimeTest) {
  uint64 idleTime = kuint64max;
  chromeos::XIdle idle;
  if (idle.getIdleTime(&idleTime))
    ASSERT_NE(idleTime, kuint64max);
  else
    ASSERT_EQ(idleTime, kuint64max);
}

}  // namespace login_manager
