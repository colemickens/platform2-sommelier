// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/clock.h"

namespace power_manager {

Clock::Clock() {}

Clock::~Clock() {}

base::TimeTicks Clock::GetCurrentTime() {
  return !current_time_for_testing_.is_null() ? current_time_for_testing_ :
      base::TimeTicks::Now();
}

base::Time Clock::GetCurrentWallTime() {
  return !current_wall_time_for_testing_.is_null() ?
      current_wall_time_for_testing_ : base::Time::Now();
}

}  // namespace power_manager
