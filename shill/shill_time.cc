// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_time.h"

#include <string.h>
#include <time.h>

#include <base/strings/stringprintf.h>

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
  struct timeval now_monotonic = (const struct timeval) { 0 };
  struct timeval now_wall_clock = (const struct timeval) { 0 };
  struct tm local_time = (const struct tm) { 0 };
  string wall_clock_string;

  GetTimeMonotonic(&now_monotonic);
  GetTimeOfDay(&now_wall_clock, NULL);
  localtime_r(&now_wall_clock.tv_sec, &local_time);
  wall_clock_string = FormatTime(local_time, now_wall_clock.tv_usec);

  return Timestamp(now_monotonic, wall_clock_string);
}

// static
string Time::FormatTime(const struct tm &date_time, suseconds_t usec) {
  char date_time_string[64];
  size_t date_time_length;
  date_time_length = strftime(date_time_string, sizeof(date_time_string),
                              "%Y-%m-%dT%H:%M:%S %z", &date_time);

  // Stitch in the microseconds, to provider finer resolution than
  // strftime allows.
  string full_string = "<unknown>";
  char *split_pos = static_cast<char *>(
      memchr(date_time_string, ' ', sizeof(date_time_string)));
  if (date_time_length && date_time_length < sizeof(date_time_string) &&
      split_pos) {
    *split_pos = '\0';
    full_string =
        base::StringPrintf("%s.%06ld%s", date_time_string, usec, split_pos+1);
  }

  return full_string;
}

time_t Time::GetSecondsSinceEpoch() const {
  return time(NULL);
}

}  // namespace shill
