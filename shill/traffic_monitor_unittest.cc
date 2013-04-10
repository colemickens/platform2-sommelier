// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/traffic_monitor.h"

#include <base/bind.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "shill/mock_device.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_ipconfig.h"
#include "shill/mock_socket_info_reader.h"
#include "shill/nice_mock_control.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::string;
using std::vector;
using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::Test;

namespace shill {

class TrafficMonitorTest : public Test {
 public:
  static const string kLocalIpAddr;
  static const uint16 kLocalPort1;
  static const uint16 kLocalPort2;
  static const uint16 kLocalPort3;
  static const uint16 kLocalPort4;
  static const uint16 kLocalPort5;
  static const string kRemoteIpAddr;
  static const uint16 kRemotePort;
  static const uint64 kTxQueueLength1;
  static const uint64 kTxQueueLength2;
  static const uint64 kTxQueueLength3;
  static const uint64 kTxQueueLength4;

  TrafficMonitorTest()
      : device_(new MockDevice(&control_,
                               &dispatcher_,
                               reinterpret_cast<Metrics *>(NULL),
                               reinterpret_cast<Manager *>(NULL),
                               "netdev0",
                               "00:11:22:33:44:55",
                               1)),
        ipconfig_(new MockIPConfig(&control_, "netdev0")),
        mock_socket_info_reader_(new MockSocketInfoReader),
        monitor_(device_, &dispatcher_),
        local_addr_(IPAddress::kFamilyIPv4),
        remote_addr_(IPAddress::kFamilyIPv4) {
    local_addr_.SetAddressFromString(kLocalIpAddr);
    remote_addr_.SetAddressFromString(kRemoteIpAddr);
  }

  MOCK_METHOD0(OnNoOutgoingPackets, void());

 protected:
  virtual void SetUp() {
    monitor_.socket_info_reader_.reset(
        mock_socket_info_reader_);  // Passes ownership

    device_->set_ipconfig(ipconfig_);
    ipconfig_properties_.address = kLocalIpAddr;
    EXPECT_CALL(*ipconfig_.get(), properties())
        .WillRepeatedly(ReturnRef(ipconfig_properties_));
  }

  void VerifyStopped() {
    EXPECT_TRUE(monitor_.sample_traffic_callback_.IsCancelled());
    EXPECT_EQ(0, monitor_.accummulated_failure_samples_);
  }

  void VerifyStarted() {
    EXPECT_FALSE(monitor_.sample_traffic_callback_.IsCancelled());
  }

  void SetupMockSocketInfos(const vector<SocketInfo> &socket_infos) {
    mock_socket_infos_ = socket_infos;
    EXPECT_CALL(*mock_socket_info_reader_, LoadTcpSocketInfo(_))
        .WillRepeatedly(
            Invoke(this, &TrafficMonitorTest::MockLoadTcpSocketInfo));
  }

  bool MockLoadTcpSocketInfo(vector<SocketInfo> *info_list) {
    *info_list = mock_socket_infos_;
    return true;
  }

  string FormatIPPort(const IPAddress &ip, const uint16 port) {
    return StringPrintf("%s:%d", ip.ToString().c_str(), port);
  }

  NiceMockControl control_;
  NiceMock<MockEventDispatcher> dispatcher_;
  scoped_refptr<MockDevice> device_;
  scoped_refptr<MockIPConfig> ipconfig_;
  IPConfig::Properties ipconfig_properties_;
  MockSocketInfoReader *mock_socket_info_reader_;
  TrafficMonitor monitor_;
  vector<SocketInfo> mock_socket_infos_;
  IPAddress local_addr_;
  IPAddress remote_addr_;
};

// static
const string TrafficMonitorTest::kLocalIpAddr = "127.0.0.1";
const uint16 TrafficMonitorTest::kLocalPort1 = 1234;
const uint16 TrafficMonitorTest::kLocalPort2 = 2345;
const uint16 TrafficMonitorTest::kLocalPort3 = 3456;
const uint16 TrafficMonitorTest::kLocalPort4 = 4567;
const uint16 TrafficMonitorTest::kLocalPort5 = 4567;
const string TrafficMonitorTest::kRemoteIpAddr = "192.168.1.1";
const uint16 TrafficMonitorTest::kRemotePort = 5678;
const uint64 TrafficMonitorTest::kTxQueueLength1 = 111;
const uint64 TrafficMonitorTest::kTxQueueLength2 = 222;
const uint64 TrafficMonitorTest::kTxQueueLength3 = 333;
const uint64 TrafficMonitorTest::kTxQueueLength4 = 444;

TEST_F(TrafficMonitorTest, StartAndStop) {
  // Stop without start
  monitor_.Stop();
  VerifyStopped();

  // Normal start
  monitor_.Start();
  VerifyStarted();

  // Stop after start
  monitor_.Stop();
  VerifyStopped();

  // Stop again without start
  monitor_.Stop();
  VerifyStopped();
}

TEST_F(TrafficMonitorTest, BuildIPPortToTxQueueLengthValid) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  TrafficMonitor::IPPortToTxQueueLengthMap tx_queue_lengths;
  monitor_.BuildIPPortToTxQueueLength(socket_infos, &tx_queue_lengths);
  EXPECT_EQ(1, tx_queue_lengths.size());
  string ip_port = FormatIPPort(local_addr_, TrafficMonitorTest::kLocalPort1);
  EXPECT_EQ(TrafficMonitorTest::kTxQueueLength1, tx_queue_lengths[ip_port]);
}

TEST_F(TrafficMonitorTest, BuildIPPortToTxQueueLengthInvalidDevice) {
  vector<SocketInfo> socket_infos;
  IPAddress foreign_ip_addr(IPAddress::kFamilyIPv4);
  foreign_ip_addr.SetAddressFromString("192.167.1.1");
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 foreign_ip_addr,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  TrafficMonitor::IPPortToTxQueueLengthMap tx_queue_lengths;
  monitor_.BuildIPPortToTxQueueLength(socket_infos, &tx_queue_lengths);
  EXPECT_EQ(0, tx_queue_lengths.size());
}

TEST_F(TrafficMonitorTest, BuildIPPortToTxQueueLengthZero) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 0,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  TrafficMonitor::IPPortToTxQueueLengthMap tx_queue_lengths;
  monitor_.BuildIPPortToTxQueueLength(socket_infos, &tx_queue_lengths);
  EXPECT_EQ(0, tx_queue_lengths.size());
}

TEST_F(TrafficMonitorTest, BuildIPPortToTxQueueLengthInvalidConnectionState) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateSynSent,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  TrafficMonitor::IPPortToTxQueueLengthMap tx_queue_lengths;
  monitor_.BuildIPPortToTxQueueLength(socket_infos, &tx_queue_lengths);
  EXPECT_EQ(0, tx_queue_lengths.size());
}

TEST_F(TrafficMonitorTest, BuildIPPortToTxQueueLengthInvalidTimerState) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateNoTimerPending));
  TrafficMonitor::IPPortToTxQueueLengthMap tx_queue_lengths;
  monitor_.BuildIPPortToTxQueueLength(socket_infos, &tx_queue_lengths);
  EXPECT_EQ(0, tx_queue_lengths.size());
}

TEST_F(TrafficMonitorTest, BuildIPPortToTxQueueLengthMultipleEntries) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateSynSent,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateNoTimerPending));
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort2,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength2,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort3,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength3,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort4,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength4,
                 0,
                 SocketInfo::kTimerStateNoTimerPending));
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort5,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 0,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  TrafficMonitor::IPPortToTxQueueLengthMap tx_queue_lengths;
  monitor_.BuildIPPortToTxQueueLength(socket_infos, &tx_queue_lengths);
  EXPECT_EQ(2, tx_queue_lengths.size());
  string ip_port = FormatIPPort(local_addr_, TrafficMonitorTest::kLocalPort2);
  EXPECT_EQ(kTxQueueLength2, tx_queue_lengths[ip_port]);
  ip_port = FormatIPPort(local_addr_, TrafficMonitorTest::kLocalPort3);
  EXPECT_EQ(kTxQueueLength3, tx_queue_lengths[ip_port]);
}

TEST_F(TrafficMonitorTest, SampleTrafficStuckTxQueueSameQueueLength) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  monitor_.set_tcp_out_traffic_not_routed_callback(
      Bind(&TrafficMonitorTest::OnNoOutgoingPackets, Unretained(this)));
  EXPECT_CALL(*this, OnNoOutgoingPackets()).Times(0);
  monitor_.SampleTraffic();
  Mock::VerifyAndClearExpectations(this);

  // Mimic same queue length by using same mock socket info.
  EXPECT_CALL(*this, OnNoOutgoingPackets());
  monitor_.SampleTraffic();
  Mock::VerifyAndClearExpectations(this);

  // Perform another sampling pass and make sure the callback is only
  // triggered once.
  EXPECT_CALL(*this, OnNoOutgoingPackets()).Times(0);
  monitor_.SampleTraffic();
}

TEST_F(TrafficMonitorTest, SampleTrafficStuckTxQueueIncreasingQueueLength) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  monitor_.set_tcp_out_traffic_not_routed_callback(
      Bind(&TrafficMonitorTest::OnNoOutgoingPackets, Unretained(this)));
  EXPECT_CALL(*this, OnNoOutgoingPackets()).Times(0);
  monitor_.SampleTraffic();
  Mock::VerifyAndClearExpectations(this);

  socket_infos.clear();
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1 + 1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  EXPECT_CALL(*this, OnNoOutgoingPackets());
  monitor_.SampleTraffic();
}

TEST_F(TrafficMonitorTest, SampleTrafficStuckTxQueueVariousQueueLengths) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength2,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  monitor_.set_tcp_out_traffic_not_routed_callback(
      Bind(&TrafficMonitorTest::OnNoOutgoingPackets, Unretained(this)));
  EXPECT_CALL(*this, OnNoOutgoingPackets()).Times(0);
  monitor_.SampleTraffic();
  Mock::VerifyAndClearExpectations(this);

  socket_infos.clear();
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  EXPECT_CALL(*this, OnNoOutgoingPackets()).Times(0);
  monitor_.SampleTraffic();
  Mock::VerifyAndClearExpectations(this);

  socket_infos.clear();
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength2,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  EXPECT_CALL(*this, OnNoOutgoingPackets());
  monitor_.SampleTraffic();
}

TEST_F(TrafficMonitorTest, SampleTrafficUnstuckTxQueueZeroQueueLength) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  monitor_.set_tcp_out_traffic_not_routed_callback(
      Bind(&TrafficMonitorTest::OnNoOutgoingPackets, Unretained(this)));
  EXPECT_CALL(*this, OnNoOutgoingPackets()).Times(0);
  monitor_.SampleTraffic();

  socket_infos.clear();
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 0,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  monitor_.SampleTraffic();
  EXPECT_EQ(0, monitor_.accummulated_failure_samples_);
}

TEST_F(TrafficMonitorTest, SampleTrafficUnstuckTxQueueNoConnection) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  monitor_.set_tcp_out_traffic_not_routed_callback(
      Bind(&TrafficMonitorTest::OnNoOutgoingPackets, Unretained(this)));
  EXPECT_CALL(*this, OnNoOutgoingPackets()).Times(0);
  monitor_.SampleTraffic();

  socket_infos.clear();
  SetupMockSocketInfos(socket_infos);
  monitor_.SampleTraffic();
  EXPECT_EQ(0, monitor_.accummulated_failure_samples_);
}

TEST_F(TrafficMonitorTest, SampleTrafficUnstuckTxQueueStateChanged) {
  vector<SocketInfo> socket_infos;
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateEstablished,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 TrafficMonitorTest::kTxQueueLength1,
                 0,
                 SocketInfo::kTimerStateRetransmitTimerPending));
  SetupMockSocketInfos(socket_infos);
  monitor_.set_tcp_out_traffic_not_routed_callback(
      Bind(&TrafficMonitorTest::OnNoOutgoingPackets, Unretained(this)));
  EXPECT_CALL(*this, OnNoOutgoingPackets()).Times(0);
  monitor_.SampleTraffic();

  socket_infos.clear();
  socket_infos.push_back(
      SocketInfo(SocketInfo::kConnectionStateClose,
                 local_addr_,
                 TrafficMonitorTest::kLocalPort1,
                 remote_addr_,
                 TrafficMonitorTest::kRemotePort,
                 0,
                 0,
                 SocketInfo::kTimerStateNoTimerPending));
  SetupMockSocketInfos(socket_infos);
  monitor_.SampleTraffic();
  EXPECT_EQ(0, monitor_.accummulated_failure_samples_);
}

}  // namespace shill
