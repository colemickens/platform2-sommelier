// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/hostapd_monitor.h"

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <shill/net/io_handler.h>

#include "apmanager/mock_event_dispatcher.h"

using base::Bind;
using base::Unretained;
using ::testing::_;

namespace {
  const char kStationMac[] = "00:11:22:33:44:55";
  const char kHostapdEventStationConnected[] =
      "<2>AP-STA-CONNECTED 00:11:22:33:44:55";
  const char kHostapdEventStationDisconnected[] =
      "<2>AP-STA-DISCONNECTED 00:11:22:33:44:55";
}  // namespace

namespace apmanager {

class HostapdEventCallbackObserver {
 public:
  HostapdEventCallbackObserver()
      : event_callback_(
          Bind(&HostapdEventCallbackObserver::OnEventCallback,
               Unretained(this))) {}
  virtual ~HostapdEventCallbackObserver() {}

  MOCK_METHOD2(OnEventCallback,
               void(HostapdMonitor::Event event, const std::string& data));

  const HostapdMonitor::EventCallback event_callback() {
    return event_callback_;
  }

 private:
  HostapdMonitor::EventCallback event_callback_;

  DISALLOW_COPY_AND_ASSIGN(HostapdEventCallbackObserver);
};

class HostapdMonitorTest : public testing::Test {
 public:
  HostapdMonitorTest()
      : hostapd_monitor_(observer_.event_callback(), "", ""),
        event_dispatcher_(MockEventDispatcher::GetInstance()) {}

  virtual void SetUp() {
    hostapd_monitor_.event_dispatcher_ = event_dispatcher_;
  }

  void Start() {
    hostapd_monitor_.Start();
  }

  void ParseMessage(shill::InputData* data) {
    hostapd_monitor_.ParseMessage(data);
  }

 protected:
  HostapdEventCallbackObserver observer_;
  HostapdMonitor hostapd_monitor_;
  MockEventDispatcher* event_dispatcher_;
};

TEST_F(HostapdMonitorTest, Start) {
  EXPECT_CALL(*event_dispatcher_, PostTask(_)).Times(1);
  Start();

  // Monitor already started, nothing to be done.
  EXPECT_CALL(*event_dispatcher_, PostTask(_)).Times(0);
  Start();
}

TEST_F(HostapdMonitorTest, StationConnected) {
  shill::InputData data;
  data.buf = reinterpret_cast<unsigned char*>(
      const_cast<char*>(kHostapdEventStationConnected));
  data.len = strlen(kHostapdEventStationConnected);
  EXPECT_CALL(observer_,
              OnEventCallback(HostapdMonitor::kStationConnected,
                              kStationMac)).Times(1);
  ParseMessage(&data);
}

TEST_F(HostapdMonitorTest, StationDisconnected) {
  shill::InputData data;
  data.buf = reinterpret_cast<unsigned char*>(
      const_cast<char*>(kHostapdEventStationDisconnected));
  data.len = strlen(kHostapdEventStationDisconnected);
  EXPECT_CALL(observer_,
              OnEventCallback(HostapdMonitor::kStationDisconnected,
                              kStationMac)).Times(1);
  ParseMessage(&data);
}

}  // namespace apmanager
