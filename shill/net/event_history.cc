// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/event_history.h"

#include <time.h>

#include <deque>

#include "shill/accessor_interface.h"
#include "shill/net/shill_time.h"

using std::deque;

namespace shill {

void EventHistory::RecordEvent() {
  RecordEventInternal(time_->GetNow());
}

void EventHistory::ExpireEventsBefore(int seconds_ago,
                                      bool count_suspend_time) {
  ExpireEventsBeforeInternal(seconds_ago, time_->GetNow(), count_suspend_time);
}

void EventHistory::RecordEventAndExpireEventsBefore(int seconds_ago,
                                                    bool count_suspend_time) {
  Timestamp now = time_->GetNow();
  RecordEventInternal(now);
  ExpireEventsBeforeInternal(seconds_ago, now, count_suspend_time);
}

Strings EventHistory::ExtractWallClockToStrings() const {
  Strings strings;
  for (deque<Timestamp>::const_iterator it = events_.begin();
       it != events_.end(); ++it) {
    strings.push_back(it->wall_clock);
  }
  return strings;
}

void EventHistory::RecordEventInternal(Timestamp now) {
  events_.push_back(now);
  while (!events_.empty() && max_events_specified_ &&
         (events_.size() > static_cast<size_t>(max_events_saved_))) {
    events_.pop_front();
  }
}

void EventHistory::ExpireEventsBeforeInternal(int seconds_ago, Timestamp now,
                                              bool count_suspend_time) {
  struct timeval period = (const struct timeval){seconds_ago};
  while (!events_.empty()) {
    struct timeval elapsed = {0, 0};
    if (count_suspend_time) {
      timersub(&now.boottime, &events_.front().boottime, &elapsed);
    } else {
      timersub(&now.monotonic, &events_.front().monotonic, &elapsed);
    }
    if (timercmp(&elapsed, &period, < )) {
      break;
    }
    events_.pop_front();
  }
}

}  // namespace shill
