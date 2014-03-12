// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_CLOCK_H_
#define POWER_MANAGER_COMMON_CLOCK_H_

#include <base/basictypes.h>
#include <base/time/time.h>

namespace power_manager {

// Simple class that allows tests to control the time.
//
// Classes should create a Clock member, provide a getter method that
// returns a pointer to it or some other means to call the
// set_current_*time_for_testing() setters, and then call GetCurrentTime()
// instead of base::TimeTicks::Now() and GetCurrentWallTime() instead of
// base::Time::Now().
class Clock {
 public:
  Clock();
  ~Clock();

  void set_current_time_for_testing(base::TimeTicks now) {
    current_time_for_testing_ = now;
  }
  void set_current_wall_time_for_testing(base::Time now) {
    current_wall_time_for_testing_ = now;
  }
  void set_time_step_for_testing(base::TimeDelta step) {
    time_step_for_testing_ = step;
  }

  // Returns the last-set monotonically-increasing time, or the actual time
  // if |current_time_for_testing_| is unset.
  base::TimeTicks GetCurrentTime();

  // Returns the last-set wall time, or the actual time if
  // |current_wall_time_for_testing_| is unset.
  base::Time GetCurrentWallTime();

 private:
  base::TimeTicks current_time_for_testing_;
  base::Time current_wall_time_for_testing_;

  // Amount of time that |current_*time_for_testing_| should be advanced by each
  // successive call to GetCurrent*Time().
  base::TimeDelta time_step_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(Clock);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_CLOCK_H_

