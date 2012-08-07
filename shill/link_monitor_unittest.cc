// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/link_monitor.h"

#include <base/bind.h>
#include <gtest/gtest.h>

#include "shill/arp_packet.h"
#include "shill/ip_address.h"
#include "shill/mock_arp_client.h"
#include "shill/mock_control.h"
#include "shill/mock_connection.h"
#include "shill/mock_device_info.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_log.h"
#include "shill/mock_metrics.h"
#include "shill/mock_sockets.h"
#include "shill/mock_time.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::Unretained;
using testing::_;
using testing::AnyNumber;
using testing::HasSubstr;
using testing::Invoke;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {
const int kInterfaceIndex = 123;
const char kLocalIPAddress[] = "10.0.1.1";
const uint8 kLocalMACAddress[] = { 0, 1, 2, 3, 4, 5 };
const char kRemoteIPAddress[] = "10.0.1.2";
const uint8 kRemoteMACAddress[] = { 6, 7, 8, 9, 10, 11 };
}  // namespace {}


class LinkMonitorForTest : public LinkMonitor {
 public:
  LinkMonitorForTest(const ConnectionRefPtr &connection,
                     EventDispatcher *dispatcher,
                     Metrics *metrics,
                     DeviceInfo *device_info)
      : LinkMonitor(connection, dispatcher, metrics, device_info,
                    Bind(&LinkMonitorForTest::FailureCallback,
                         Unretained(this))) {}

  virtual ~LinkMonitorForTest() {}

  MOCK_METHOD0(CreateClient, bool());
  MOCK_METHOD0(FailureCallback, void());
};

MATCHER_P4(IsArpRequest, local_ip, remote_ip, local_mac, remote_mac, "") {
  return
      local_ip.Equals(arg.local_ip_address()) &&
      remote_ip.Equals(arg.remote_ip_address()) &&
      local_mac.Equals(arg.local_mac_address()) &&
      remote_mac.Equals(arg.remote_mac_address());
}

class LinkMonitorTest : public Test {
 public:
  LinkMonitorTest()
      : device_info_(
            &control_,
            reinterpret_cast<EventDispatcher*>(NULL),
            reinterpret_cast<Metrics*>(NULL),
            reinterpret_cast<Manager*>(NULL)),
        connection_(new StrictMock<MockConnection>(&device_info_)),
        monitor_(connection_, &dispatcher_, &metrics_, &device_info_),
        client_(NULL),
        next_client_(new StrictMock<MockArpClient>()),
        gateway_ip_(IPAddress::kFamilyIPv4),
        local_ip_(IPAddress::kFamilyIPv4),
        gateway_mac_(kRemoteMACAddress, arraysize(kRemoteMACAddress)),
        local_mac_(kLocalMACAddress, arraysize(kLocalMACAddress)),
        zero_mac_(arraysize(kLocalMACAddress)),
        link_scope_logging_was_enabled_(false) {}
  virtual ~LinkMonitorTest() {}

  virtual void SetUp() {
    link_scope_logging_was_enabled_ = SLOG_IS_ON(Link, 0);
    if (!link_scope_logging_was_enabled_) {
      ScopeLogger::GetInstance()->EnableScopesByName("link");
      ScopeLogger::GetInstance()->set_verbose_level(4);
    }
    monitor_.time_ = &time_;
    EXPECT_CALL(time_, GetTimeMonotonic(_))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(time_val_), Return(0)));
    EXPECT_TRUE(local_ip_.SetAddressFromString(kLocalIPAddress));
    EXPECT_CALL(*connection_, local()).WillRepeatedly(ReturnRef(local_ip_));
    EXPECT_TRUE(gateway_ip_.SetAddressFromString(kRemoteIPAddress));
    EXPECT_CALL(*connection_, gateway()).WillRepeatedly(ReturnRef(gateway_ip_));
  }

  virtual void TearDown() {
    if (!link_scope_logging_was_enabled_) {
      ScopeLogger::GetInstance()->EnableScopesByName("-link");
      ScopeLogger::GetInstance()->set_verbose_level(0);
    }
  }

  void AdvanceTime(unsigned int time_ms) {
    struct timeval adv_time = { time_ms/1000, (time_ms % 1000) * 1000 };
    timeradd(&time_val_, &adv_time, &time_val_);
    EXPECT_CALL(time_, GetTimeMonotonic(_))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(time_val_), Return(0)));
  }

  bool CreateMockClient() {
    EXPECT_FALSE(monitor_.arp_client_.get());
    if (client_) {
      Mock::VerifyAndClearExpectations(client_);
    }
    client_ = next_client_;
    next_client_ = new StrictMock<MockArpClient>();
    monitor_.arp_client_.reset(client_);
    return true;
  }

 protected:
  void ExpectReset() {
    EXPECT_FALSE(monitor_.GetResponseTimeMilliseconds());
    EXPECT_FALSE(GetArpClient());
    EXPECT_TRUE(GetSendRequestCallback().IsCancelled());
    EXPECT_EQ(0, GetBroadcastFailureCount());
    EXPECT_EQ(0, GetUnicastFailureCount());
    EXPECT_FALSE(IsUnicast());
  }
  const ArpClient *GetArpClient() { return monitor_.arp_client_.get(); }
  void TriggerRequestTimer() {
    GetSendRequestCallback().callback().Run();
  }
  const base::CancelableClosure &GetSendRequestCallback() {
    return monitor_.send_request_callback_;
  }
  unsigned int GetBroadcastFailureCount() {
    return monitor_.broadcast_failure_count_;
  }
  unsigned int GetUnicastFailureCount() {
    return monitor_.unicast_failure_count_;
  }
  bool IsUnicast() { return monitor_.is_unicast_; }
  unsigned int GetTestPeriodMilliseconds() {
    return LinkMonitor::kTestPeriodMilliseconds;
  }
  unsigned int GetFailureThreshold() {
    return LinkMonitor::kFailureThreshold;
  }
  unsigned int GetMaxResponseSampleFilterDepth() {
    return LinkMonitor::kMaxResponseSampleFilterDepth;
  }
  void ExpectTransmit(bool is_unicast) {
    const ByteString &destination_mac = is_unicast ? gateway_mac_ : zero_mac_;
    if (monitor_.arp_client_.get()) {
      EXPECT_EQ(client_, monitor_.arp_client_.get());
      EXPECT_CALL(*client_, TransmitRequest(
          IsArpRequest(local_ip_, gateway_ip_, local_mac_, destination_mac)))
          .WillOnce(Return(true));
    } else {
      EXPECT_CALL(monitor_, CreateClient())
          .WillOnce(Invoke(this, &LinkMonitorTest::CreateMockClient));
      EXPECT_CALL(*next_client_, TransmitRequest(
          IsArpRequest(local_ip_, gateway_ip_, local_mac_, destination_mac)))
          .WillOnce(Return(true));
    }
    EXPECT_CALL(dispatcher_, PostDelayedTask(_, GetTestPeriodMilliseconds()));
  }
  void SendNextRequest() {
    EXPECT_CALL(monitor_, CreateClient())
        .WillOnce(Invoke(this, &LinkMonitorTest::CreateMockClient));
    EXPECT_CALL(*next_client_, TransmitRequest(_)).WillOnce(Return(true));
    EXPECT_CALL(dispatcher_, PostDelayedTask(_, GetTestPeriodMilliseconds()));
    TriggerRequestTimer();
  }
  void ExpectNoTransmit() {
    MockArpClient *client =
      monitor_.arp_client_.get() ? client_ : next_client_;
    EXPECT_CALL(*client, TransmitRequest(_)).Times(0);
  }
  void StartMonitor() {
    EXPECT_CALL(device_info_, GetMACAddress(0, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(local_mac_), Return(true)));
    ExpectTransmit(false);
    EXPECT_TRUE(monitor_.Start());
    EXPECT_TRUE(GetArpClient());
    EXPECT_FALSE(IsUnicast());
    EXPECT_FALSE(GetSendRequestCallback().IsCancelled());
  }
  bool SimulateReceiveReply(ArpPacket *packet, ByteString *sender) {
    packet->set_local_ip_address(rx_packet_.local_ip_address());
    packet->set_remote_ip_address(rx_packet_.remote_ip_address());
    packet->set_local_mac_address(rx_packet_.local_mac_address());
    packet->set_remote_mac_address(rx_packet_.remote_mac_address());
    return true;
  }
  void ReceiveResponse(const IPAddress &local_ip,
                       const ByteString &local_mac,
                       const IPAddress &remote_ip,
                       const ByteString &remote_mac) {
    rx_packet_.set_local_ip_address(local_ip);
    rx_packet_.set_local_mac_address(local_mac);
    rx_packet_.set_remote_ip_address(remote_ip);
    rx_packet_.set_remote_mac_address(remote_mac);

    EXPECT_CALL(*client_, ReceiveReply(_, _))
        .WillOnce(Invoke(this, &LinkMonitorTest::SimulateReceiveReply));
    monitor_.ReceiveResponse(0);
  }
  void ReceiveCorrectResponse() {
    ReceiveResponse(local_ip_, local_mac_, gateway_ip_, gateway_mac_);
  }

  MockEventDispatcher dispatcher_;
  StrictMock<MockMetrics> metrics_;
  MockControl control_;
  NiceMock<MockDeviceInfo> device_info_;
  scoped_refptr<MockConnection> connection_;
  StrictMock<LinkMonitorForTest> monitor_;
  MockTime time_;
  struct timeval time_val_;
  // This is owned by the LinkMonitor, and only tracked here for EXPECT*().
  MockArpClient *client_;
  // This is owned by the test until it is handed to the LinkMonitorForTest
  // and placed // in client_ above.
  MockArpClient *next_client_;
  IPAddress gateway_ip_;
  IPAddress local_ip_;
  ByteString gateway_mac_;
  ByteString local_mac_;
  ByteString zero_mac_;
  ArpPacket rx_packet_;
  bool link_scope_logging_was_enabled_;
};


TEST_F(LinkMonitorTest, Constructor) {
  ExpectReset();
}

TEST_F(LinkMonitorTest, StartFailedGetMACAddress) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("Could not get local MAC address"))).Times(1);
  EXPECT_CALL(device_info_, GetMACAddress(0, _)).WillOnce(Return(false));
  EXPECT_CALL(metrics_, SendEnumToUMA(
      HasSubstr("LinkMonitorFailure"), Metrics::kLinkMonitorMacAddressNotFound,
      _));
  EXPECT_CALL(monitor_, CreateClient()).Times(0);
  EXPECT_FALSE(monitor_.Start());
  ExpectReset();
}

TEST_F(LinkMonitorTest, StartFailedCreateClient) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("Failed to start ARP client"))).Times(1);
  EXPECT_CALL(metrics_, SendEnumToUMA(
      HasSubstr("LinkMonitorFailure"), Metrics::kLinkMonitorClientStartFailure,
      _));
  EXPECT_CALL(device_info_, GetMACAddress(0, _)).WillOnce(Return(true));
  EXPECT_CALL(monitor_, CreateClient()).WillOnce(Return(false));
  EXPECT_FALSE(monitor_.Start());
  ExpectReset();
}

TEST_F(LinkMonitorTest, StartFailedTransmitRequest) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("Failed to send ARP"))).Times(1);
  EXPECT_CALL(metrics_, SendEnumToUMA(
      HasSubstr("LinkMonitorFailure"), Metrics::kLinkMonitorTransmitFailure,
      _));
  EXPECT_CALL(device_info_, GetMACAddress(0, _)).WillOnce(Return(true));
  EXPECT_CALL(monitor_, CreateClient())
      .WillOnce(Invoke(this, &LinkMonitorTest::CreateMockClient));
  EXPECT_CALL(*next_client_, TransmitRequest(_)).WillOnce(Return(false));
  EXPECT_FALSE(monitor_.Start());
  ExpectReset();
}

TEST_F(LinkMonitorTest, StartSuccess) {
  StartMonitor();
}

TEST_F(LinkMonitorTest, Stop) {
  StartMonitor();
  monitor_.Stop();
  ExpectReset();
}

TEST_F(LinkMonitorTest, ReplyReception) {
  StartMonitor();
  const unsigned int kResponseTime = 1234;
  AdvanceTime(kResponseTime);
  ScopedMockLog log;

  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("not for our IP"))).Times(1);
  ReceiveResponse(gateway_ip_, local_mac_, gateway_ip_, gateway_mac_);
  Mock::VerifyAndClearExpectations(&log);

  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("not for our MAC"))).Times(1);
  ReceiveResponse(local_ip_, gateway_mac_, gateway_ip_, gateway_mac_);
  Mock::VerifyAndClearExpectations(&log);

  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("not from the gateway"))).Times(1);
  ReceiveResponse(local_ip_, local_mac_, local_ip_, gateway_mac_);
  Mock::VerifyAndClearExpectations(&log);

  EXPECT_TRUE(GetArpClient());
  EXPECT_FALSE(monitor_.GetResponseTimeMilliseconds());
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("Found gateway"))).Times(1);
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), kResponseTime,
       _, _, _)).Times(1);
  ReceiveCorrectResponse();
  EXPECT_FALSE(GetArpClient());
  EXPECT_EQ(kResponseTime, monitor_.GetResponseTimeMilliseconds());
  EXPECT_TRUE(IsUnicast());
}

TEST_F(LinkMonitorTest, TimeoutBroadcast) {
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), GetTestPeriodMilliseconds(),
      _, _, _)).Times(GetFailureThreshold());
  StartMonitor();
  for (unsigned int i = 1; i < GetFailureThreshold(); ++i) {
    ExpectTransmit(false);
    TriggerRequestTimer();
    EXPECT_FALSE(IsUnicast());
    EXPECT_EQ(i, GetBroadcastFailureCount());
    EXPECT_EQ(0, GetUnicastFailureCount());
    EXPECT_EQ(GetTestPeriodMilliseconds(),
              monitor_.GetResponseTimeMilliseconds());
  }
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("monitor has reached the failure threshold"))).Times(1);
  EXPECT_CALL(metrics_, SendEnumToUMA(
      HasSubstr("LinkMonitorFailure"),
      Metrics::kLinkMonitorFailureThresholdReached, _));
  EXPECT_FALSE(GetSendRequestCallback().IsCancelled());
  ExpectNoTransmit();
  EXPECT_CALL(monitor_, FailureCallback());
  TriggerRequestTimer();
  ExpectReset();
}

TEST_F(LinkMonitorTest, TimeoutUnicast) {
  StartMonitor();
  // Successful broadcast receptions.
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), 0, _, _, _))
      .Times(GetFailureThreshold());
  // Unsuccessful unicast receptions.
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), GetTestPeriodMilliseconds(),
      _, _, _)).Times(GetFailureThreshold());
  ReceiveCorrectResponse();
  for (unsigned int i = 1; i < GetFailureThreshold(); ++i) {
    // Failed unicast ARP.
    ExpectTransmit(true);
    TriggerRequestTimer();

    // Successful broadcast ARP.
    ExpectTransmit(false);
    TriggerRequestTimer();
    ReceiveCorrectResponse();

    EXPECT_EQ(0, GetBroadcastFailureCount());
    EXPECT_EQ(i, GetUnicastFailureCount());
  }
  // Last unicast ARP transmission.
  ExpectTransmit(true);
  TriggerRequestTimer();

  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("monitor has reached the failure threshold"))).Times(1);
  EXPECT_CALL(metrics_, SendEnumToUMA(
      HasSubstr("LinkMonitorFailure"),
      Metrics::kLinkMonitorFailureThresholdReached, _));
  EXPECT_FALSE(GetSendRequestCallback().IsCancelled());
  ExpectNoTransmit();
  EXPECT_CALL(monitor_, FailureCallback());
  TriggerRequestTimer();
  ExpectReset();
}

TEST_F(LinkMonitorTest, Average) {
  const unsigned int kSamples[] = { 200, 950, 1200, 4096, 5000,
                                   86, 120, 3060, 842, 750 };
  const unsigned int filter_depth = GetMaxResponseSampleFilterDepth();
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), _, _, _, _))
      .Times(arraysize(kSamples));
  ASSERT_GT(arraysize(kSamples), filter_depth);
  StartMonitor();
  unsigned int i = 0;
  unsigned int sum = 0;
  for (; i < filter_depth; ++i) {
    AdvanceTime(kSamples[i]);
    ReceiveCorrectResponse();
    sum += kSamples[i];
    EXPECT_EQ(sum / (i + 1), monitor_.GetResponseTimeMilliseconds());
    SendNextRequest();
  }
  for (; i < arraysize(kSamples); ++i) {
    AdvanceTime(kSamples[i]);
    ReceiveCorrectResponse();
    sum = (sum + kSamples[i]) * filter_depth / (filter_depth + 1);
    EXPECT_EQ(sum / filter_depth, monitor_.GetResponseTimeMilliseconds());
    SendNextRequest();
  }
}

TEST_F(LinkMonitorTest, ImpulseResponse) {
  const unsigned int kNormalValue = 50;
  const unsigned int kExceptionalValue = 5000;
  const unsigned int filter_depth = GetMaxResponseSampleFilterDepth();
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), _, _, _, _))
      .Times(AnyNumber());
  StartMonitor();
  for (unsigned int i = 0; i < filter_depth * 2; ++i) {
    AdvanceTime(kNormalValue);
    ReceiveCorrectResponse();
    EXPECT_EQ(kNormalValue, monitor_.GetResponseTimeMilliseconds());
    SendNextRequest();
  }
  AdvanceTime(kExceptionalValue);
  ReceiveCorrectResponse();
  // Our expectation is that an impulse input will be a
  // impulse_height / (filter_depth + 1) increase to the running average.
  unsigned int expected_impulse_response =
      kNormalValue + (kExceptionalValue - kNormalValue) / (filter_depth + 1);
  EXPECT_EQ(expected_impulse_response, monitor_.GetResponseTimeMilliseconds());
  SendNextRequest();

  // From here, if we end up continuing to receive normal values, our
  // running average should decay backwards to the normal value.
  const unsigned int failsafe = 100;
  unsigned int last_value = monitor_.GetResponseTimeMilliseconds();
  for (unsigned int i = 0; i < failsafe && last_value != kNormalValue; ++i) {
    AdvanceTime(kNormalValue);
    ReceiveCorrectResponse();
    // We should advance monotonically (but not necessarily linearly)
    // back towards the normal value.
    EXPECT_GE(last_value, monitor_.GetResponseTimeMilliseconds());
    SendNextRequest();
    last_value = monitor_.GetResponseTimeMilliseconds();
  }
  EXPECT_EQ(kNormalValue, last_value);
}

}  // namespace shill
