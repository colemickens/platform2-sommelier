// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/traffic_monitor.h"

#include <base/bind.h>
#include <gtest/gtest.h>

#include "shill/mock_device.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/nice_mock_control.h"

using testing::NiceMock;
using testing::Return;
using testing::Test;

namespace shill {

class CallbackChecker {
 public:
  CallbackChecker() : invocation_count_(0) {}
  ~CallbackChecker() {}
  void InvokeCallback() { ++invocation_count_; }
  int invocation_count() const { return invocation_count_; }

 private:
  int invocation_count_;

  DISALLOW_COPY_AND_ASSIGN(CallbackChecker);
};

class TrafficMonitorTest : public Test {
 public:
  TrafficMonitorTest()
      : device_(new MockDevice(&control_,
                               &dispatcher_,
                               reinterpret_cast<Metrics *>(NULL),
                               reinterpret_cast<Manager *>(NULL),
                               "netdev0",
                               "00:11:22:33:44:55",
                               1)),
        monitor_(device_, &dispatcher_) {
  }

 protected:
  void ExpectStopped() {
    EXPECT_EQ(0, monitor_.last_receive_byte_count_);
    EXPECT_EQ(0, monitor_.last_transmit_byte_count_);
    EXPECT_EQ(0, monitor_.no_traffic_count_);
    EXPECT_EQ(0, monitor_.no_incoming_traffic_count_);
    EXPECT_FALSE(monitor_.no_incoming_traffic_callback_invoked_);
  }

  NiceMockControl control_;
  NiceMock<MockEventDispatcher> dispatcher_;
  scoped_refptr<MockDevice> device_;
  TrafficMonitor monitor_;
};

TEST_F(TrafficMonitorTest, StartAndStop) {
  monitor_.Stop();  // Stop without start
  ExpectStopped();

  EXPECT_CALL(*device_, GetReceiveByteCount()).WillOnce(Return(100));
  EXPECT_CALL(*device_, GetTransmitByteCount()).WillOnce(Return(200));
  monitor_.Start();
  EXPECT_EQ(100, monitor_.last_receive_byte_count_);
  EXPECT_EQ(200, monitor_.last_transmit_byte_count_);
  EXPECT_EQ(0, monitor_.no_traffic_count_);
  EXPECT_EQ(0, monitor_.no_incoming_traffic_count_);
  EXPECT_FALSE(monitor_.no_incoming_traffic_callback_invoked_);

  monitor_.Stop();  // Stop after start
  ExpectStopped();

  monitor_.Stop();  // Stop again without start
  ExpectStopped();
}

TEST_F(TrafficMonitorTest, SampleTraffic) {
  const struct {
    uint64 receive_byte_count;
    uint64 transmit_byte_count;
    int expected_no_traffic_count;
    int expected_no_incoming_traffic_count;
    int expected_callback_invocation_count;
    bool expected_no_incoming_traffic_callback_invoked;
  } kTestVector[] = {
    { 1, 1, 0, 0, 0, false },   // Initial condition
    { 1, 2, 0, 1, 0, false },   // No incoming traffic
    { 2, 3, 0, 0, 0, false },   // Reset detection due to incoming traffic
    { 2, 4, 0, 1, 0, false },   // No incoming traffic
    { 2, 4, 1, 1, 0, false },   // No traffic
    { 2, 5, 0, 2, 1, true },    // Invoke "no incoming traffic" callback
    { 2, 6, 0, 3, 1, true },    // Do not invoke callback again
    { 3, 7, 0, 0, 1, false },   // Reset detection due to incoming traffic
    { 3, 8, 0, 1, 1, false },   // No incoming traffic
    { 3, 9, 0, 2, 2, true },    // Invoke "no incoming traffic" callback
    { 3, 9, 1, 2, 2, true },    // No traffic
    { 3, 9, 2, 2, 2, true },
    { 3, 9, 3, 2, 2, true },
    { 3, 9, 4, 2, 2, true },
    { 3, 9, 5, 2, 2, true },
    { 3, 9, 6, 2, 2, true },
    { 3, 9, 7, 2, 2, true },
    { 3, 9, 8, 2, 2, true },
    { 3, 9, 1, 0, 2, false },   // Reset detection due to no traffic
    { 3, 9, 2, 0, 2, false },
    { 3, 9, 3, 0, 2, false },
    { 3, 9, 4, 0, 2, false },
    { 3, 9, 5, 0, 2, false },
    { 3, 9, 6, 0, 2, false },
    { 3, 9, 7, 0, 2, false },
    { 3, 9, 8, 0, 2, false },
    { 4, 9, 0, 0, 2, false },   // Reset detection due to incoming traffic
    { 4, 9, 1, 0, 2, false },
    { 4, 9, 2, 0, 2, false },
    { 4, 9, 3, 0, 2, false },
    { 4, 9, 4, 0, 2, false },
    { 4, 9, 5, 0, 2, false },
    { 4, 9, 6, 0, 2, false },
    { 4, 9, 7, 0, 2, false },
    { 4, 9, 8, 0, 2, false },
    { 4, 10, 0, 1, 2, false },  // Only outgoing traffic
  };

  CallbackChecker callback_checker;
  monitor_.set_no_incoming_traffic_callback(
      base::Bind(&CallbackChecker::InvokeCallback,
                 base::Unretained(&callback_checker)));

  ExpectStopped();
  EXPECT_FALSE(monitor_.no_incoming_traffic_callback_invoked_);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestVector); ++i) {
    EXPECT_CALL(*device_, GetReceiveByteCount())
        .WillOnce(Return(kTestVector[i].receive_byte_count));
    EXPECT_CALL(*device_, GetTransmitByteCount())
        .WillOnce(Return(kTestVector[i].transmit_byte_count));

    if (i == 0)
      monitor_.Start();
    else
      monitor_.SampleTraffic();

    EXPECT_EQ(kTestVector[i].receive_byte_count,
              monitor_.last_receive_byte_count_);
    EXPECT_EQ(kTestVector[i].transmit_byte_count,
              monitor_.last_transmit_byte_count_);
    EXPECT_EQ(kTestVector[i].expected_no_traffic_count,
              monitor_.no_traffic_count_);
    EXPECT_EQ(kTestVector[i].expected_no_incoming_traffic_count,
              monitor_.no_incoming_traffic_count_);
    EXPECT_EQ(kTestVector[i].expected_callback_invocation_count,
              callback_checker.invocation_count());
    EXPECT_EQ(kTestVector[i].expected_no_incoming_traffic_callback_invoked,
              monitor_.no_incoming_traffic_callback_invoked_);
  }
}

}  // namespace shill
