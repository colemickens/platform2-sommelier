// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_EVENT_HISTORY_H_
#define SHILL_NET_EVENT_HISTORY_H_

#include <deque>

#include <base/macros.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/net/shill_time.h"
#include "shill/shill_export.h"

namespace shill {

// EventHistory is a list of timestamps tracking the occurence of one or more
// events. Events are ordered from earliest to latest. |max_events_saved|
// can optionally be provided to limit the number of event timestamps saved
// at any one time.
class SHILL_EXPORT EventHistory {
 public:
  EventHistory() : max_events_specified_(false), time_(Time::GetInstance()) {}
  explicit EventHistory(int max_events_saved)
      : max_events_specified_(true),
        max_events_saved_(max_events_saved),
        time_(Time::GetInstance()) {}

  // Records the current event by adding the current time to the list.
  // If |event_limit_specificed_| and the size of |events_| is larger than
  // |max_events_saved_|, event timestamps are removed in FIFO order until the
  // size of |events_| is equal to |max_events_saved_|.
  void RecordEvent();

  // Start at the head of |events_| and remove all entries that occurred
  // more than |seconds_ago| prior to the current time. We count suspend time
  // (i.e. use the boottime clock) if |count_suspend_time| is true, otherwise,
  // we do not necessarily do so (i.e. use the monotic clock).
  void ExpireEventsBefore(int seconds_ago, bool count_suspend_time);

  // Records the current event by adding the current time to the list, and uses
  // this same timestamp to remove all entries that occurred more than
  // |seconds_ago|. |count_suspend_time| is used to choose between the monotonic
  // and boottime clock, as described above in EventHistory::ExpireEventsBefore.
  void RecordEventAndExpireEventsBefore(int seconds_ago,
                                        bool count_suspend_time);

  // Returns a vector of human-readable strings representing each timestamp in
  // |events_|.
  Strings ExtractWallClockToStrings() const;

  size_t Size() const { return events_.size(); }
  bool Empty() { return events_.empty(); }
  Timestamp Front() { return events_.front(); }

 private:
  friend class EventHistoryTest;
  friend class ServiceTest;  // RecordEventInternal, time_

  void RecordEventInternal(Timestamp now);

  void ExpireEventsBeforeInternal(int seconds_ago, Timestamp now,
                                  bool count_suspend_time);

  bool max_events_specified_;
  int max_events_saved_;
  std::deque<Timestamp> events_;
  Time *time_;

  DISALLOW_COPY_AND_ASSIGN(EventHistory);
};

}  // namespace shill

#endif  // SHILL_NET_EVENT_HISTORY_H_
