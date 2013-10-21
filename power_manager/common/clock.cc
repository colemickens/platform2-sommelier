// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/clock.h"

namespace power_manager {

Clock::Clock() {}

Clock::~Clock() {}

base::TimeTicks Clock::GetCurrentTime() {
  if (current_time_for_testing_.is_null())
    return base::TimeTicks::Now();

  current_time_for_testing_ += time_step_for_testing_;
  return current_time_for_testing_;
}

base::Time Clock::GetCurrentWallTime() {
  if (current_wall_time_for_testing_.is_null())
    return base::Time::Now();

  current_wall_time_for_testing_ += time_step_for_testing_;
  return current_wall_time_for_testing_;
}

}  // namespace power_manager
