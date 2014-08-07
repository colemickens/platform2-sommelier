// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MAC80211_MONITOR_H_
#define SHILL_MAC80211_MONITOR_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace shill {

class Metrics;

class Mac80211Monitor {
 public:
  struct QueueState {
    QueueState(size_t queue_number_in,
               uint32_t stop_flags_in,
               size_t queue_length_in)
    : queue_number(queue_number_in), stop_flags(stop_flags_in),
      queue_length(queue_length_in) {}

    size_t queue_number;
    uint32_t stop_flags;
    size_t queue_length;
  };

  Mac80211Monitor(const std::string &link_name, size_t queue_length_limit,
                  Metrics *metrics);
  virtual ~Mac80211Monitor();

 private:
  friend class Mac80211MonitorTest;
  FRIEND_TEST(Mac80211MonitorTest, CheckAreQueuesStuckMultipleReasons);
  FRIEND_TEST(Mac80211MonitorTest, CheckAreQueuesStuckMultipleQueues);
  FRIEND_TEST(Mac80211MonitorTest, CheckAreQueuesStuckNotStuck);
  FRIEND_TEST(Mac80211MonitorTest, CheckAreQueuesStuckQueueLength);
  FRIEND_TEST(Mac80211MonitorTest,
              CheckAreQueuesStuckQueueLengthIgnoresUnstopped);
  FRIEND_TEST(Mac80211MonitorTest, CheckAreQueuesStuckSingleReason);
  FRIEND_TEST(Mac80211MonitorTest, ParseQueueStateBadInput);
  FRIEND_TEST(Mac80211MonitorTest, ParseQueueStateSimple);
  FRIEND_TEST(Mac80211MonitorTest, ParseQueueStateStopped);

  static const size_t kMaxQueueStateSizeBytes;
  // Values must be kept in sync with ieee80211_i.h.
  enum QueueStopReason {
    kStopReasonDriver,
    kStopReasonPowerSave,
    kStopReasonChannelSwitch,
    kStopReasonAggregation,
    kStopReasonSuspend,
    kStopReasonBufferAdd,
    kStopReasonChannelTypeChange,
    kStopReasonMax = kStopReasonChannelTypeChange
  };
  enum QueueStopFlag {
    kStopFlagDriver = 1 << kStopReasonDriver,
    kStopFlagPowerSave = 1 << kStopReasonPowerSave,
    kStopFlagChannelSwitch = 1 << kStopReasonChannelSwitch,
    kStopFlagAggregation = 1 << kStopReasonAggregation,
    kStopFlagSuspend = 1 << kStopReasonSuspend,
    kStopFlagBufferAdd = 1 << kStopReasonBufferAdd,
    kStopFlagChannelTypeChange = 1 << kStopReasonChannelTypeChange,
    kStopFlagInvalid
  };

  // Check |queue_states|, to determine if any queues are stuck.
  // Returns a bitmask of QueueStopFlags. A flag will be set if
  // any of the queues has that flag set, and is non-empty.
  // A return value if 0 indicates no queues are stuck.
  uint32_t CheckAreQueuesStuck(const std::vector<QueueState> &queue_states);

  static std::vector<QueueState> ParseQueueState(
      const std::string &state_string);
  static QueueStopFlag GetFlagForReason(QueueStopReason reason);

  const std::string link_name_;
  size_t queue_length_limit_;
  Metrics *metrics_;

  DISALLOW_COPY_AND_ASSIGN(Mac80211Monitor);
};

}  // namespace shill

#endif  // SHILL_MAC80211_MONITOR_H_
