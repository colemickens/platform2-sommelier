// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/realtime_clock_alarm.h"

#include <sys/timerfd.h>

#include <base/files/file_util.h>
#include <base/logging.h>

namespace power_manager {
namespace system {

RealtimeClockAlarm::RealtimeClockAlarm(base::TimeDelta duration)
    : duration_(duration),
      alarm_fd_(-1) {
}

RealtimeClockAlarm::~RealtimeClockAlarm() {
  if (alarm_fd_ != -1) {
    // Calling timerfd_settime with 0 for each member disarms the alarm.
    itimerspec blank_time {};
    timerfd_settime(alarm_fd_, 0, &blank_time, NULL);

    close(alarm_fd_);
  }
}

bool RealtimeClockAlarm::Arm() {
  // If the duration is zero or negative, we can't arm this timer.
  if (duration_ <= base::TimeDelta()) {
    LOG(ERROR) << "Tried to set alarm with nonsensical time limit "
               << duration_.ToInternalValue();
    return false;
  }

  // Set up a timerfd for the given duration. CLOCK_REALTIME_ALARM makes
  // sure that we will be woken up upon expiry.
  alarm_fd_ = timerfd_create(CLOCK_REALTIME_ALARM, TFD_NONBLOCK);
  if (alarm_fd_ < 0) {
    PLOG(WARNING) << "Could not create alarm";
    return false;
  }

  // Set the timeout.
  itimerspec alarm_time {};
  alarm_time.it_value.tv_sec = duration_.InSeconds();
  alarm_time.it_value.tv_nsec = (duration_.InMicroseconds() % 1000000) * 1000;
  if (timerfd_settime(alarm_fd_, 0, &alarm_time, NULL) < 0) {
    PLOG(WARNING) << "Could not set alarm";
    return false;
  }

  return true;
}

bool RealtimeClockAlarm::HasExpired() {
  // User might have forgotten to set the alarm.
  if (alarm_fd_ == -1)
    return false;

  // If we can read a uint64_t from the timer, we know the timer has
  // expired at least once since we last checked (and in this case,
  // exactly once, since the timer is set in one-shot mode in Arm().)
  char tick[sizeof(uint64_t)];
  return base::ReadFromFD(alarm_fd_, tick, sizeof(tick));
}

}  // namespace system
}  // namespace power_manager
