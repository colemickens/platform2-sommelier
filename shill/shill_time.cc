// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_time.h"

#include <time.h>

#include <base/stringprintf.h>

using std::string;

namespace shill {

namespace {

// As Time may be instantiated by MemoryLogMessage during a callback of
// AtExitManager, it needs to be a leaky singleton to avoid
// AtExitManager::RegisterCallback() from potentially being called within a
// callback of AtExitManager, which will lead to a crash. Making Time leaky is
// fine as it does not need to clean up or release any resource at destruction.
base::LazyInstance<Time>::Leaky g_time = LAZY_INSTANCE_INITIALIZER;

}  // namespace

Time::Time() { }

Time::~Time() { }

Time* Time::GetInstance() {
  return g_time.Pointer();
}

int Time::GetTimeMonotonic(struct timeval *tv) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return -1;
  }

  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;
  return 0;
}

int Time::GetTimeOfDay(struct timeval *tv, struct timezone *tz) {
  return gettimeofday(tv, tz);
}

Timestamp Time::GetNow() {
  struct timeval now_monotonic = (const struct timeval){ 0 };
  GetTimeMonotonic(&now_monotonic);
  struct timeval now_wall_clock = (const struct timeval){ 0 };
  GetTimeOfDay(&now_wall_clock, NULL);
  struct tm local_time = (const struct tm){ 0 };
  localtime_r(&now_wall_clock.tv_sec, &local_time);
  // There're two levels of formatting -- first most of the timestamp is printed
  // into |format|, then the resulting string is used to insert the microseconds
  // (note the %%06u format string).
  char format[64];
  size_t length = strftime(format, sizeof(format),
                           "%Y-%m-%dT%H:%M:%S.%%06u%z", &local_time);
  string wall_clock = "<unknown>";
  if (length && length < sizeof(format)) {
    wall_clock = base::StringPrintf(format, now_wall_clock.tv_usec);
  }
  return Timestamp(now_monotonic, wall_clock);
}

}  // namespace shill
