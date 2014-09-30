// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/link_monitor.h"

#include <string>

#include <base/bind.h>
#include <gtest/gtest.h>

#include "shill/arp_packet.h"
#include "shill/byte_string.h"
#include "shill/ip_address.h"
#include "shill/logging.h"
#include "shill/mock_arp_client.h"
#include "shill/mock_connection.h"
#include "shill/mock_control.h"
#include "shill/mock_device_info.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_log.h"
#include "shill/mock_metrics.h"
#include "shill/mock_sockets.h"
#include "shill/mock_time.h"

using base::Bind;
using base::Unretained;
using std::string;
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
const char kLocalIPAddress[] = "10.0.1.1";
const uint8_t kLocalMACAddress[] = { 0, 1, 2, 3, 4, 5 };
const char kRemoteIPAddress[] = "10.0.1.2";
const uint8_t kRemoteMACAddress[] = { 6, 7, 8, 9, 10, 11 };
}  // namespace


class LinkMonitorForTest : public LinkMonitor {
 public:
  LinkMonitorForTest(const ConnectionRefPtr &connection,
                     EventDispatcher *dispatcher,
                     Metrics *metrics,
                     DeviceInfo *device_info)
      : LinkMonitor(connection, dispatcher, metrics, device_info,
                    Bind(&LinkMonitorForTest::FailureCallbackHandler,
                         Unretained(this)),
                    Bind(&LinkMonitorForTest::GatewayChangeCallbackHandler,
                         Unretained(this))) {}

  virtual ~LinkMonitorForTest() {}

  MOCK_METHOD0(CreateClient, bool());

  // This does not override any methods in LinkMonitor, but is used here
  // in unit tests to support expectations when the LinkMonitor notifies
  // its client of a failure.
  MOCK_METHOD0(FailureCallbackHandler, void());

  // This does not override any methods in LinkMonitor, but is used here
  // in unit tests to support expectations when the LinkMonitor notifies
  // its client of a gateway mac address change.
  MOCK_METHOD0(GatewayChangeCallbackHandler, void());
};

MATCHER_P4(IsArpRequest, local_ip, remote_ip, local_mac, remote_mac, "") {
  if (local_ip.Equals(arg.local_ip_address()) &&
      remote_ip.Equals(arg.remote_ip_address()) &&
      local_mac.Equals(arg.local_mac_address()) &&
      remote_mac.Equals(arg.remote_mac_address()))
    return true;

  if (!local_ip.Equals(arg.local_ip_address())) {
    *result_listener << "Local IP '" << arg.local_ip_address().ToString()
                     << "' (wanted '" << local_ip.ToString() << "').";
  }

  if (!remote_ip.Equals(arg.remote_ip_address())) {
    *result_listener << "Remote IP '" << arg.remote_ip_address().ToString()
                     << "' (wanted '" << remote_ip.ToString() << "').";
  }

  if (!local_mac.Equals(arg.local_mac_address())) {
    *result_listener << "Local MAC '" << arg.local_mac_address().HexEncode()
                     << "' (wanted " << local_mac.HexEncode() << ")'.";
  }

  if (!remote_mac.Equals(arg.remote_mac_address())) {
    *result_listener << "Remote MAC '" << arg.remote_mac_address().HexEncode()
                     << "' (wanted " << remote_mac.HexEncode() << ")'.";
  }

  return false;
}

class LinkMonitorTest : public Test {
 public:
  LinkMonitorTest()
      : metrics_(&dispatcher_),
        device_info_(&control_, nullptr, nullptr, nullptr),
        connection_(new StrictMock<MockConnection>(&device_info_)),
        monitor_(connection_, &dispatcher_, &metrics_, &device_info_),
        client_(nullptr),
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
    time_val_.tv_sec = 0;
    time_val_.tv_usec = 0;
    EXPECT_CALL(time_, GetTimeMonotonic(_))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(time_val_), Return(0)));
    EXPECT_TRUE(local_ip_.SetAddressFromString(kLocalIPAddress));
    EXPECT_CALL(*connection_, local()).WillRepeatedly(ReturnRef(local_ip_));
    EXPECT_TRUE(gateway_ip_.SetAddressFromString(kRemoteIPAddress));
    EXPECT_CALL(*connection_, gateway()).WillRepeatedly(ReturnRef(gateway_ip_));
    EXPECT_CALL(*connection_, technology())
        .WillRepeatedly(Return(Technology::kEthernet));
  }

  virtual void TearDown() {
    if (!link_scope_logging_was_enabled_) {
      ScopeLogger::GetInstance()->EnableScopesByName("-link");
      ScopeLogger::GetInstance()->set_verbose_level(0);
    }
  }

  void AdvanceTime(int time_ms) {
    struct timeval adv_time = {
      static_cast<time_t>(time_ms/1000),
      static_cast<time_t>((time_ms % 1000) * 1000) };
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

  string HardwareAddressToString(const ByteString &address) {
    return LinkMonitor::HardwareAddressToString(address);
  }

 protected:
  void ExpectReset() {
    EXPECT_FALSE(monitor_.GetResponseTimeMilliseconds());
    EXPECT_FALSE(GetArpClient());
    EXPECT_TRUE(GetSendRequestCallback().IsCancelled());
    EXPECT_EQ(0, GetBroadcastFailureCount());
    EXPECT_EQ(0, GetUnicastFailureCount());
    EXPECT_EQ(0, GetBroadcastSuccessCount());
    EXPECT_EQ(0, GetUnicastSuccessCount());
    EXPECT_FALSE(IsUnicast());
    EXPECT_FALSE(GatewaySupportsUnicastArp());
  }
  const ArpClient *GetArpClient() { return monitor_.arp_client_.get(); }
  void TriggerRequestTimer() {
    GetSendRequestCallback().callback().Run();
  }
  const base::CancelableClosure &GetSendRequestCallback() {
    return monitor_.send_request_callback_;
  }
  int GetBroadcastFailureCount() {
    return monitor_.broadcast_failure_count_;
  }
  int GetUnicastFailureCount() {
    return monitor_.unicast_failure_count_;
  }
  int GetBroadcastSuccessCount() {
    return monitor_.broadcast_success_count_;
  }
  int GetUnicastSuccessCount() {
    return monitor_.unicast_success_count_;
  }
  bool IsUnicast() { return monitor_.is_unicast_; }
  bool GatewaySupportsUnicastArp() {
    return monitor_.gateway_supports_unicast_arp_;
  }
  void SetGatewaySupportsUnicastArp(bool gateway_supports_unicast_arp) {
    monitor_.gateway_supports_unicast_arp_ = gateway_supports_unicast_arp;
  }
  int GetCurrentTestPeriodMilliseconds() {
    return monitor_.test_period_milliseconds_;
  }
  int GetDefaultTestPeriodMilliseconds() {
    return LinkMonitor::kDefaultTestPeriodMilliseconds;
  }
  size_t GetFailureThreshold() {
    return LinkMonitor::kFailureThreshold;
  }
  size_t GetUnicastReplyReliabilityThreshold() {
    return LinkMonitor::kUnicastReplyReliabilityThreshold;
  }
  int GetFastTestPeriodMilliseconds() {
    return LinkMonitor::kFastTestPeriodMilliseconds;
  }
  int GetMaxResponseSampleFilterDepth() {
    return LinkMonitor::kMaxResponseSampleFilterDepth;
  }
  void ExpectTransmit(bool is_unicast, int transmit_period_milliseconds) {
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
    EXPECT_CALL(dispatcher_,
                PostDelayedTask(_, transmit_period_milliseconds));
  }
  void SendNextRequest() {
    EXPECT_CALL(monitor_, CreateClient())
        .WillOnce(Invoke(this, &LinkMonitorTest::CreateMockClient));
    EXPECT_CALL(*next_client_, TransmitRequest(_)).WillOnce(Return(true));
    EXPECT_CALL(dispatcher_,
                PostDelayedTask(_, GetCurrentTestPeriodMilliseconds()));
    TriggerRequestTimer();
  }
  void ExpectNoTransmit() {
    MockArpClient *client =
      monitor_.arp_client_.get() ? client_ : next_client_;
    EXPECT_CALL(*client, TransmitRequest(_)).Times(0);
  }
  void ExpectRestart(int transmit_period_milliseconds) {
    EXPECT_CALL(device_info_, GetMACAddress(0, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(local_mac_), Return(true)));
    // Can't just use ExpectTransmit, because that depends on state
    // that changes during Stop.
    EXPECT_CALL(monitor_, CreateClient())
        .WillOnce(Invoke(this, &LinkMonitorTest::CreateMockClient));
    EXPECT_CALL(*next_client_, TransmitRequest(
        IsArpRequest(local_ip_, gateway_ip_, local_mac_, zero_mac_)))
        .WillOnce(Return(true));
    EXPECT_CALL(dispatcher_,
                PostDelayedTask(_, transmit_period_milliseconds));
  }
  void StartMonitor() {
    EXPECT_CALL(device_info_, GetMACAddress(0, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(local_mac_), Return(true)));
    ExpectTransmit(false, GetDefaultTestPeriodMilliseconds());
    EXPECT_TRUE(monitor_.Start());
    EXPECT_TRUE(GetArpClient());
    EXPECT_FALSE(IsUnicast());
    EXPECT_FALSE(GetSendRequestCallback().IsCancelled());
  }
  void ReportResume() {
    monitor_.OnAfterResume();
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
    ReceiveResponse(gateway_ip_, gateway_mac_, local_ip_, local_mac_);
  }
  void RunUnicastResponseCycle(int cycle_count,
                               bool should_respond_to_unicast_probes,
                               bool should_count_failures) {
    // This method expects the LinkMonitor to be in a state where it
    // is waiting for a broadcast response.  It also returns with the
    // LinkMonitor in the same state.
    // Successful receptions.
    EXPECT_CALL(metrics_, SendToUMA(
        HasSubstr("LinkMonitorResponseTimeSample"), 0, _, _, _))
        .Times(cycle_count * (should_respond_to_unicast_probes ? 2 : 1));
    // Unsuccessful unicast receptions.
    EXPECT_CALL(metrics_, SendToUMA(
        HasSubstr("LinkMonitorResponseTimeSample"),
        GetDefaultTestPeriodMilliseconds(),
        _, _, _)).Times(cycle_count *
                        (should_respond_to_unicast_probes ? 0 : 1));

    // Account for any successes / failures before we started.
    int expected_broadcast_success_count = GetBroadcastSuccessCount();
    int expected_unicast_success_count = GetUnicastSuccessCount();
    int expected_unicast_failure_count = GetUnicastFailureCount();

    for (int i = 0; i < cycle_count; ++i) {
      // Respond to the pending broadcast request.
      ReceiveCorrectResponse();

      // Unicast ARP.
      ExpectTransmit(true, GetDefaultTestPeriodMilliseconds());
      TriggerRequestTimer();
      if (should_respond_to_unicast_probes) ReceiveCorrectResponse();

      // Initiate broadcast ARP.
      ExpectTransmit(false, GetDefaultTestPeriodMilliseconds());
      TriggerRequestTimer();

      ++expected_broadcast_success_count;
      if (should_respond_to_unicast_probes) {
        ++expected_unicast_success_count;
        expected_unicast_failure_count = 0;
      } else {
        if (should_count_failures) {
          ++expected_unicast_failure_count;
        }
        expected_unicast_success_count = 0;
      }
      EXPECT_EQ(expected_unicast_failure_count, GetUnicastFailureCount());
      EXPECT_EQ(expected_unicast_success_count, GetUnicastSuccessCount());
      EXPECT_EQ(0, GetBroadcastFailureCount());
      EXPECT_EQ(expected_broadcast_success_count, GetBroadcastSuccessCount());
    }
  }
  bool IsGatewayFound() {
    return monitor_.IsGatewayFound();
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
  const int kResponseTime = 1234;
  AdvanceTime(kResponseTime);
  ScopedMockLog log;

  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("not for our IP"))).Times(1);
  ReceiveResponse(gateway_ip_, gateway_mac_, gateway_ip_, local_mac_);
  Mock::VerifyAndClearExpectations(&log);

  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("not for our MAC"))).Times(1);
  ReceiveResponse(gateway_ip_, gateway_mac_, local_ip_, gateway_mac_);
  Mock::VerifyAndClearExpectations(&log);

  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("not from the gateway"))).Times(1);
  ReceiveResponse(local_ip_, gateway_mac_, local_ip_, local_mac_);
  Mock::VerifyAndClearExpectations(&log);

  EXPECT_TRUE(GetArpClient());
  EXPECT_FALSE(monitor_.GetResponseTimeMilliseconds());
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("Found gateway"))).Times(1);
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), kResponseTime,
       _, _, _)).Times(1);
  EXPECT_CALL(monitor_, GatewayChangeCallbackHandler()).Times(1);
  ReceiveCorrectResponse();
  EXPECT_FALSE(GetArpClient());
  EXPECT_EQ(kResponseTime, monitor_.GetResponseTimeMilliseconds());
  EXPECT_TRUE(IsUnicast());
}

TEST_F(LinkMonitorTest, TimeoutBroadcast) {
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"),
      GetDefaultTestPeriodMilliseconds(),
      _, _, _)).Times(GetFailureThreshold());
  StartMonitor();
  // This value doesn't match real life (the timer in this scenario
  // should advance by LinkMonitor::kDefaultTestPeriodMilliseconds),
  // but this demonstrates the LinkMonitorSecondsToFailure independent
  // from the response-time figures.
  const int kTimeIncrement = 1000;
  for (size_t i = 1; i < GetFailureThreshold(); ++i) {
    ExpectTransmit(false, GetDefaultTestPeriodMilliseconds());
    AdvanceTime(kTimeIncrement);
    TriggerRequestTimer();
    EXPECT_FALSE(IsUnicast());
    EXPECT_EQ(i, GetBroadcastFailureCount());
    EXPECT_EQ(0, GetUnicastFailureCount());
    EXPECT_EQ(0, GetBroadcastSuccessCount());
    EXPECT_EQ(0, GetUnicastSuccessCount());
    EXPECT_EQ(GetDefaultTestPeriodMilliseconds(),
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
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorSecondsToFailure"),
      kTimeIncrement * GetFailureThreshold() / 1000, _, _, _));
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("BroadcastErrorsAtFailure"), GetFailureThreshold(), _, _, _));
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("UnicastErrorsAtFailure"), 0, _, _, _));
  EXPECT_FALSE(GetSendRequestCallback().IsCancelled());
  ExpectNoTransmit();
  EXPECT_CALL(monitor_, FailureCallbackHandler());
  AdvanceTime(kTimeIncrement);
  TriggerRequestTimer();
  ExpectReset();
}

TEST_F(LinkMonitorTest, TimeoutUnicast) {
  StartMonitor();

  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("monitor has reached the failure threshold"))).Times(0);
  EXPECT_CALL(metrics_, SendEnumToUMA(
      HasSubstr("LinkMonitorFailure"),
      Metrics::kLinkMonitorFailureThresholdReached, _)).Times(0);
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorSecondsToFailure"), _, _, _, _)).Times(0);
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("BroadcastErrorsAtFailure"), _, _, _, _)).Times(0);
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("UnicastErrorsAtFailure"), _, _, _, _)).Times(0);
  EXPECT_CALL(monitor_, GatewayChangeCallbackHandler()).Times(1);

  // Unicast failures should not cause LinkMonitor errors if we haven't
  // noted the gateway as reliably replying to unicast ARP messages.  Test
  // this by doing threshold - 1 successful unicast responses, followed
  // by a ton of unicast failures.
  RunUnicastResponseCycle(GetUnicastReplyReliabilityThreshold() - 1,
                          true, false);
  EXPECT_EQ(GetUnicastReplyReliabilityThreshold() - 1,
            GetUnicastSuccessCount());
  RunUnicastResponseCycle(GetFailureThreshold() +
                          GetUnicastReplyReliabilityThreshold(), false, false);
  EXPECT_FALSE(GetSendRequestCallback().IsCancelled());
  EXPECT_FALSE(GatewaySupportsUnicastArp());
  EXPECT_EQ(0, GetUnicastSuccessCount());
  EXPECT_EQ(0, GetUnicastFailureCount());

  // Cross the the unicast reliability threshold.
  RunUnicastResponseCycle(GetUnicastReplyReliabilityThreshold() - 1,
                          true, false);
  EXPECT_CALL(log,
      Log(_, _, HasSubstr("Unicast failures will now count")));
  EXPECT_FALSE(GatewaySupportsUnicastArp());
  RunUnicastResponseCycle(1, true, false);
  EXPECT_TRUE(GatewaySupportsUnicastArp());

  // Induce one less failures than will cause a link monitor failure, and
  // confirm that these failures are counted.
  RunUnicastResponseCycle(GetFailureThreshold() - 1, false, true);
  EXPECT_EQ(GetFailureThreshold() - 1, GetUnicastFailureCount());

  Mock::VerifyAndClearExpectations(&log);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());

  // Induce a final broadcast success followed by a unicast failure.
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), 0, _, _, _));
  ReceiveCorrectResponse();
  ExpectTransmit(true, GetDefaultTestPeriodMilliseconds());
  TriggerRequestTimer();
  EXPECT_FALSE(GetSendRequestCallback().IsCancelled());

  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"),
      GetDefaultTestPeriodMilliseconds(),
      _, _, _));
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("monitor has reached the failure threshold"))).Times(1);
  EXPECT_CALL(metrics_, SendEnumToUMA(
      HasSubstr("LinkMonitorFailure"),
      Metrics::kLinkMonitorFailureThresholdReached, _));
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorSecondsToFailure"), 0, _, _, _));
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("BroadcastErrorsAtFailure"), 0, _, _, _));
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("UnicastErrorsAtFailure"), GetFailureThreshold(), _, _, _));
  EXPECT_CALL(monitor_, FailureCallbackHandler());
  ExpectNoTransmit();
  TriggerRequestTimer();
  ExpectReset();
}

TEST_F(LinkMonitorTest, OnAfterResume) {
  const int kFastTestPeriodMilliseconds = GetFastTestPeriodMilliseconds();
  StartMonitor();
  Mock::VerifyAndClearExpectations(&monitor_);

  // Resume should preserve the fact that we haven't resolved the gateway's MAC,
  // as well as the fact that the gateway suppports unicast ARP.
  EXPECT_FALSE(IsGatewayFound());
  EXPECT_FALSE(GatewaySupportsUnicastArp());
  ExpectRestart(kFastTestPeriodMilliseconds);
  ReportResume();
  EXPECT_FALSE(IsGatewayFound());
  EXPECT_FALSE(GatewaySupportsUnicastArp());

  // This is the expected normal case, so let's test that.  The
  // OnAfterResumeWithoutUnicast test below verifies when this is not true.
  SetGatewaySupportsUnicastArp(true);

  // After resume, we should use the fast test period...
  ExpectRestart(kFastTestPeriodMilliseconds);
  ReportResume();
  EXPECT_EQ(kFastTestPeriodMilliseconds, GetCurrentTestPeriodMilliseconds());

  // ...and the fast period should be used for reporting failure to UMA...
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), kFastTestPeriodMilliseconds,
      _, _, _));
  ExpectTransmit(false, kFastTestPeriodMilliseconds);
  TriggerRequestTimer();

  // ...and the period should be reset after correct responses on both
  // broadcast and unicast.
  const int kResponseTime = 12;
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), kResponseTime,  _, _, _))
      .Times(2);
  EXPECT_CALL(monitor_, GatewayChangeCallbackHandler()).Times(1);
  AdvanceTime(kResponseTime);
  ReceiveCorrectResponse();
  EXPECT_EQ(GetFastTestPeriodMilliseconds(),
            GetCurrentTestPeriodMilliseconds());  // Don't change yet.
  ExpectTransmit(true, kFastTestPeriodMilliseconds);
  TriggerRequestTimer();
  AdvanceTime(kResponseTime);
  ReceiveCorrectResponse();
  EXPECT_EQ(1, GetBroadcastSuccessCount());
  EXPECT_EQ(1, GetUnicastSuccessCount());
  EXPECT_EQ(GetDefaultTestPeriodMilliseconds(),
            GetCurrentTestPeriodMilliseconds());

  // Resume should preserve the fact that we _have_ resolved the gateway's MAC.
  EXPECT_TRUE(IsGatewayFound());
  ExpectRestart(kFastTestPeriodMilliseconds);
  ReportResume();
  EXPECT_TRUE(IsGatewayFound());
  EXPECT_TRUE(GatewaySupportsUnicastArp());

  // Failure should happen just like normal.
  ExpectRestart(kFastTestPeriodMilliseconds);
  ReportResume();
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), kFastTestPeriodMilliseconds,
      _, _, _))
      .Times(GetFailureThreshold());
  EXPECT_CALL(metrics_, SendEnumToUMA(
      HasSubstr("LinkMonitorFailure"),
      Metrics::kLinkMonitorFailureThresholdReached, _));
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorSecondsToFailure"), _, _, _, _));
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("BroadcastErrorsAtFailure"), GetFailureThreshold() / 2 + 1,
      _, _, _));
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("UnicastErrorsAtFailure"), GetFailureThreshold() / 2,
      _, _, _));
  EXPECT_CALL(monitor_, FailureCallbackHandler());
  bool unicast_probe = true;
  for (size_t i = 1 ; i < GetFailureThreshold(); ++i) {
    ExpectTransmit(unicast_probe, GetFastTestPeriodMilliseconds());
    TriggerRequestTimer();
    unicast_probe = !unicast_probe;
  }
  TriggerRequestTimer();
  ExpectReset();
}

TEST_F(LinkMonitorTest, OnAfterResumeWithoutUnicast) {
  const int kFastTestPeriodMilliseconds = GetFastTestPeriodMilliseconds();
  StartMonitor();
  Mock::VerifyAndClearExpectations(&monitor_);

  // Resume should preserve the fact that we haven't resolved the gateway's MAC.
  EXPECT_FALSE(IsGatewayFound());
  EXPECT_FALSE(GatewaySupportsUnicastArp());
  ExpectRestart(kFastTestPeriodMilliseconds);
  ReportResume();
  EXPECT_FALSE(IsGatewayFound());
  EXPECT_FALSE(GatewaySupportsUnicastArp());

  // After resume, we should use the fast test period...
  ExpectRestart(kFastTestPeriodMilliseconds);
  ReportResume();
  EXPECT_EQ(kFastTestPeriodMilliseconds, GetCurrentTestPeriodMilliseconds());

  // ...and the fast period should be used for reporting failure to UMA...
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), kFastTestPeriodMilliseconds,
      _, _, _));
  ExpectTransmit(false, kFastTestPeriodMilliseconds);
  TriggerRequestTimer();

  // ...and the period should be reset after correct response on just the
  // broadcast response since the link monitor doesn't trust unicast.
  const int kResponseTime = 12;
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), kResponseTime,  _, _, _));
  AdvanceTime(kResponseTime);
  EXPECT_CALL(monitor_, GatewayChangeCallbackHandler()).Times(1);
  ReceiveCorrectResponse();
  EXPECT_EQ(GetDefaultTestPeriodMilliseconds(),
            GetCurrentTestPeriodMilliseconds());
}

TEST_F(LinkMonitorTest, Average) {
  const int kSamples[] = { 200, 950, 1200, 4096, 5000,
                           86, 120, 3060, 842, 750 };
  const size_t filter_depth = GetMaxResponseSampleFilterDepth();
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), _, _, _, _))
      .Times(arraysize(kSamples));
  EXPECT_CALL(monitor_, GatewayChangeCallbackHandler()).Times(1);
  ASSERT_GT(arraysize(kSamples), filter_depth);
  StartMonitor();
  size_t i = 0;
  int sum = 0;
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
  const int kNormalValue = 50;
  const int kExceptionalValue = 5000;
  const int filter_depth = GetMaxResponseSampleFilterDepth();
  EXPECT_CALL(metrics_, SendToUMA(
      HasSubstr("LinkMonitorResponseTimeSample"), _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(monitor_, GatewayChangeCallbackHandler()).Times(1);
  StartMonitor();
  for (int i = 0; i < filter_depth * 2; ++i) {
    AdvanceTime(kNormalValue);
    ReceiveCorrectResponse();
    EXPECT_EQ(kNormalValue, monitor_.GetResponseTimeMilliseconds());
    SendNextRequest();
  }
  AdvanceTime(kExceptionalValue);
  ReceiveCorrectResponse();
  // Our expectation is that an impulse input will be a
  // impulse_height / (filter_depth + 1) increase to the running average.
  int expected_impulse_response =
      kNormalValue + (kExceptionalValue - kNormalValue) / (filter_depth + 1);
  EXPECT_EQ(expected_impulse_response, monitor_.GetResponseTimeMilliseconds());
  SendNextRequest();

  // From here, if we end up continuing to receive normal values, our
  // running average should decay backwards to the normal value.
  const int failsafe = 100;
  int last_value = monitor_.GetResponseTimeMilliseconds();
  for (int i = 0; i < failsafe && last_value != kNormalValue; ++i) {
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

TEST_F(LinkMonitorTest, HardwareAddressToString) {
  const uint8_t address0[] = { 0, 1, 2, 3, 4, 5 };
  EXPECT_EQ("00:01:02:03:04:05",
            HardwareAddressToString(ByteString(address0, arraysize(address0))));
  const uint8_t address1[] = { 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd };
  EXPECT_EQ("88:99:aa:bb:cc:dd",
            HardwareAddressToString(ByteString(address1, arraysize(address1))));
}

}  // namespace shill
