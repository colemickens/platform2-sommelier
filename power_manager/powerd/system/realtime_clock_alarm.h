// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_REALTIME_CLOCK_ALARM_H_
#define POWER_MANAGER_POWERD_SYSTEM_REALTIME_CLOCK_ALARM_H_

#include <base/macros.h>
#include <base/time/time.h>

namespace power_manager {
namespace system {

class RealtimeClockAlarm {
 public:
  explicit RealtimeClockAlarm(base::TimeDelta duration);
  ~RealtimeClockAlarm();

  bool Arm();
  bool HasExpired();

 private:
  base::TimeDelta duration_;
  int alarm_fd_;

  DISALLOW_COPY_AND_ASSIGN(RealtimeClockAlarm);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_REALTIME_CLOCK_ALARM_H_
