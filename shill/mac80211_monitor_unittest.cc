// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mac80211_monitor.h"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using std::vector;
using ::testing::ElementsAre;

namespace shill {

typedef Mac80211Monitor::QueueState QState;

class Mac80211MonitorTest : public testing::Test {
 public:
  Mac80211MonitorTest()
      : mac80211_monitor_() {}
  virtual ~Mac80211MonitorTest() {}

 private:
  Mac80211Monitor mac80211_monitor_;
};

// Can't be in an anonymous namespace, due to ADL.
// Instead, we use static to constain visibility to this unit.
static bool operator==(const QState &a, const QState &b) {
  return a.queue_number == b.queue_number &&
      a.stop_flags == b.stop_flags &&
      a.queue_length == b.queue_length;
}

TEST_F(Mac80211MonitorTest, ParseQueueStateSimple) {
  // Single queue.
  EXPECT_THAT(Mac80211Monitor::ParseQueueState("00: 0x00000000/0\n"),
              ElementsAre(QState(0, 0, 0)));

  // Multiple queues, non-empty.
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState(
          "00: 0x00000000/10\n"
          "01: 0x00000000/20\n"),
      ElementsAre(QState(0, 0, 10), QState(1, 0, 20)));
}

TEST_F(Mac80211MonitorTest, ParseQueueStateStopped) {
  // Single queue, stopped for various reasons.
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState("00: 0x00000001/10\n"),
      ElementsAre(QState(0, Mac80211Monitor::kStopFlagDriver, 10)));
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState("00: 0x00000003/10\n"),
      ElementsAre(QState(0,
                         Mac80211Monitor::kStopFlagDriver |
                         Mac80211Monitor::kStopFlagPowerSave,
                         10)));
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState("00: 0x00000007/10\n"),
      ElementsAre(QState(0,
                         Mac80211Monitor::kStopFlagDriver |
                         Mac80211Monitor::kStopFlagPowerSave |
                         Mac80211Monitor::kStopFlagChannelSwitch,
                         10)));
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState("00: 0x0000000f/10\n"),
      ElementsAre(QState(0,
                         Mac80211Monitor::kStopFlagDriver |
                         Mac80211Monitor::kStopFlagPowerSave |
                         Mac80211Monitor::kStopFlagChannelSwitch |
                         Mac80211Monitor::kStopFlagAggregation,
                         10)));
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState("00: 0x0000001f/10\n"),
      ElementsAre(QState(0,
                         Mac80211Monitor::kStopFlagDriver |
                         Mac80211Monitor::kStopFlagPowerSave |
                         Mac80211Monitor::kStopFlagChannelSwitch |
                         Mac80211Monitor::kStopFlagAggregation |
                         Mac80211Monitor::kStopFlagSuspend,
                         10)));
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState("00: 0x0000003f/10\n"),
      ElementsAre(QState(0,
                         Mac80211Monitor::kStopFlagDriver |
                         Mac80211Monitor::kStopFlagPowerSave |
                         Mac80211Monitor::kStopFlagChannelSwitch |
                         Mac80211Monitor::kStopFlagAggregation |
                         Mac80211Monitor::kStopFlagSuspend |
                         Mac80211Monitor::kStopFlagBufferAdd,
                         10)));
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState("00: 0x0000007f/10\n"),
      ElementsAre(QState(0,
                         Mac80211Monitor::kStopFlagDriver |
                         Mac80211Monitor::kStopFlagPowerSave |
                         Mac80211Monitor::kStopFlagChannelSwitch |
                         Mac80211Monitor::kStopFlagAggregation |
                         Mac80211Monitor::kStopFlagSuspend |
                         Mac80211Monitor::kStopFlagBufferAdd |
                         Mac80211Monitor::kStopFlagChannelTypeChange,
                         10)));
}

TEST_F(Mac80211MonitorTest, ParseQueueStateBadInput) {
  // Empty input -> Empty output.
  EXPECT_TRUE(Mac80211Monitor::ParseQueueState("").empty());

  // Missing queue length for queue 0.
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState(
          "00: 0x00000000\n"
          "01: 0xffffffff/10\n"),
      ElementsAre(QState(1, 0xffffffff, 10)));

  // Missing flags for queue 0.
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState(
          "00: 0\n"
          "01: 0xffffffff/10\n"),
      ElementsAre(QState(1, 0xffffffff, 10)));

  // Bad number for queue 0.
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState(
          "aa: 0xabcdefgh/0\n"
          "01: 0xffffffff/10\n"),
      ElementsAre(QState(1, 0xffffffff, 10)));

  // Bad flags for queue 0.
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState(
          "00: 0xabcdefgh/0\n"
          "01: 0xffffffff/10\n"),
      ElementsAre(QState(1, 0xffffffff, 10)));

  // Bad length for queue 0.
  EXPECT_THAT(
      Mac80211Monitor::ParseQueueState(
          "00: 0x00000000/-1\n"
          "01: 0xffffffff/10\n"),
      ElementsAre(QState(1, 0xffffffff, 10)));
}

}  // namespace shill
