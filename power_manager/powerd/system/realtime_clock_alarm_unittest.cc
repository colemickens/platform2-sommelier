// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/realtime_clock_alarm.h"

#include <gtest/gtest.h>

namespace power_manager {
namespace system {

TEST(RealtimeClockAlarmTest, ZeroDuration) {
  base::TimeDelta d;
  RealtimeClockAlarm alarm(d);
  EXPECT_FALSE(alarm.Arm());
}

TEST(RealtimeClockAlarmTest, NegativeDuration) {
  RealtimeClockAlarm alarm(base::TimeDelta::FromSeconds(-1));
  EXPECT_FALSE(alarm.Arm());
}

TEST(RealtimeClockAlarmTest, DoesNotExpireBeforeArmed) {
  RealtimeClockAlarm alarm(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(alarm.HasExpired());
}

}  // namespace system
}  // namespace power_manager
