// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection_health_checker.h"

#include <arpa/inet.h>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "shill/mock_async_connection.h"
#include "shill/mock_connection.h"
#include "shill/mock_control.h"
#include "shill/mock_dns_client.h"
#include "shill/mock_device_info.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_sockets.h"
#include "shill/mock_socket_info_reader.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;
using std::vector;
using ::testing::DoAll;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Sequence;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::_;

namespace shill {

namespace {
const char kInterfaceName[] = "int0";
const char kIPAddress_0_0_0_0[] = "0.0.0.0";
const char kIPAddress_8_8_8_8[] = "8.8.8.8";
const char kProxyIPAddressRemote[] = "74.125.224.84";
const char kProxyIPAddressLocal[] = "192.23.34.1";
const char kProxyURLRemote[] = "http://www.google.com";
const int kProxyFD = 100;
const short kProxyPortLocal = 5540;
const short kProxyPortRemote = 80;
}  // namespace {}

MATCHER_P(IsSameIPAddress, ip_addr, "") {
  return arg.Equals(ip_addr);
}

class ConnectionHealthCheckerTest : public Test {
 public:
  ConnectionHealthCheckerTest()
      : interface_name_(kInterfaceName),
        device_info_(&control_, &dispatcher_,
                     reinterpret_cast<Metrics*>(NULL),
                     reinterpret_cast<Manager*>(NULL)),
        connection_(new MockConnection(&device_info_)),
        socket_(NULL) {}

  // Invokes
  int GetSockName(int fd, struct sockaddr *addr_out,
                        socklen_t *sockaddr_size) {
    struct sockaddr_in addr;
    EXPECT_EQ(kProxyFD, fd);
    EXPECT_GE(sizeof(sockaddr_in), *sockaddr_size);
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, kProxyIPAddressLocal, &addr.sin_addr);
    addr.sin_port = htons(kProxyPortLocal);
    memcpy(addr_out, &addr, sizeof(addr));
    *sockaddr_size = sizeof(sockaddr_in);
    return 0;
  }
  int GetSockNameReturnsIPv6(int fd, struct sockaddr *addr_out,
                        socklen_t *sockaddr_size) {
    struct sockaddr_in addr;
    EXPECT_EQ(kProxyFD, fd);
    EXPECT_GE(sizeof(sockaddr_in), *sockaddr_size);
    addr.sin_family = AF_INET6;
    inet_pton(AF_INET, kProxyIPAddressLocal, &addr.sin_addr);
    addr.sin_port = htons(kProxyPortLocal);
    memcpy(addr_out, &addr, sizeof(addr));
    *sockaddr_size = sizeof(sockaddr_in);
    return 0;
  }
  void InvokeOnConnectionComplete(bool success, int sock_fd) {
    health_checker_->OnConnectionComplete(success, sock_fd);
  }

 protected:
  void SetUp() {
    EXPECT_CALL(*connection_.get(), interface_name())
        .WillRepeatedly(ReturnRef(interface_name_));
    EXPECT_CALL(*connection_.get(), dns_servers())
        .WillOnce(ReturnRef(dns_servers_));
    health_checker_.reset(
        new ConnectionHealthChecker(
             connection_,
             &dispatcher_,
             Bind(&ConnectionHealthCheckerTest::ResultCallbackTarget,
                  Unretained(this))));

    socket_ = new StrictMock<MockSockets>();
    tcp_connection_ = new StrictMock<MockAsyncConnection>();
    socket_info_reader_ = new StrictMock<MockSocketInfoReader>();
    dns_client_ = new MockDNSClient();
    // Passes ownership for all of these.
    health_checker_->socket_.reset(socket_);
    health_checker_->tcp_connection_.reset(tcp_connection_);
    health_checker_->socket_info_reader_.reset(socket_info_reader_);
    health_checker_->dns_client_.reset(dns_client_);
  }

  void TearDown() {
    EXPECT_CALL(*tcp_connection_, Stop())
        .Times(1);
  }

  // Accessors for private data in ConnectionHealthChecker.
  const Sockets *socket() {
    return health_checker_->socket_.get();
  }
  const AsyncConnection *tcp_connection() {
    return health_checker_->tcp_connection_.get(); }
  bool health_check_in_progress() {
    return health_checker_->health_check_in_progress_;
  }
  short num_connection_attempts() {
    return health_checker_->num_connection_attempts_;
  }
  const ConnectionHealthChecker::IPAddressQueue &remote_ips() {
    return health_checker_->remote_ips_;
  }
  int MaxConnectionAttempts() {
    return ConnectionHealthChecker::kMaxConnectionAttempts;
  }

  // Mock Callbacks
  MOCK_METHOD1(ResultCallbackTarget,
               void(ConnectionHealthChecker::Result result));



  // Helper methods
  IPAddress StringToIPv4Address(const string &address_string) {
    IPAddress ip_address(IPAddress::kFamilyIPv4);
    EXPECT_TRUE(ip_address.SetAddressFromString(address_string));
    return ip_address;
  }
  // Naming: CreateSocketInfo
  //         + (Proxy/Other) : TCP connection for proxy socket / some other
  //         socket.
  //         + arg1: Pass in any SocketInfo::ConnectionState you want.
  //         + arg2: Pass in any value of transmit_queue_value you want.
  SocketInfo CreateSocketInfoOther() {
    return SocketInfo(
        SocketInfo::kConnectionStateUnknown,
        StringToIPv4Address(kIPAddress_8_8_8_8),
        0,
        StringToIPv4Address(kProxyIPAddressRemote),
        kProxyPortRemote,
        0,
        0,
        SocketInfo::kTimerStateUnknown);
  }
  SocketInfo CreateSocketInfoProxy(SocketInfo::ConnectionState state) {
    return SocketInfo(
        state,
        StringToIPv4Address(kProxyIPAddressLocal),
        kProxyPortLocal,
        StringToIPv4Address(kProxyIPAddressRemote),
        kProxyPortRemote,
        0,
        0,
        SocketInfo::kTimerStateUnknown);
  }
  SocketInfo CreateSocketInfoProxy(SocketInfo::ConnectionState state,
                                   uint64 transmit_queue_value) {
    return SocketInfo(
        state,
        StringToIPv4Address(kProxyIPAddressLocal),
        kProxyPortLocal,
        StringToIPv4Address(kProxyIPAddressRemote),
        kProxyPortRemote,
        transmit_queue_value,
        0,
        SocketInfo::kTimerStateUnknown);
  }
  void GetDNSResultFailure() {
    Error error(Error::kOperationFailed, "");
    IPAddress address(IPAddress::kFamilyUnknown);
    health_checker_->GetDNSResult(error, address);
  }
  void GetDNSResultSuccess(const IPAddress &address) {
    Error error;
    health_checker_->GetDNSResult(error, address);
  }
  void VerifyAndClearAllExpectations() {
    Mock::VerifyAndClearExpectations(this);
    Mock::VerifyAndClearExpectations(tcp_connection_);
    Mock::VerifyAndClearExpectations(socket_);
    Mock::VerifyAndClearExpectations(socket_info_reader_);
  }

  // Expectations
  void ExpectReset() {
    EXPECT_EQ(connection_.get(), health_checker_->connection_.get());
    EXPECT_EQ(&dispatcher_, health_checker_->dispatcher_);
    EXPECT_EQ(socket_, health_checker_->socket_.get());
    EXPECT_FALSE(socket_ == NULL);
    EXPECT_EQ(socket_info_reader_, health_checker_->socket_info_reader_.get());
    EXPECT_FALSE(socket_info_reader_ == NULL);
    EXPECT_FALSE(health_checker_->connection_complete_callback_.is_null());
    EXPECT_EQ(tcp_connection_, health_checker_->tcp_connection_.get());
    EXPECT_FALSE(tcp_connection_ == NULL);
    EXPECT_FALSE(health_checker_->health_check_in_progress_);
    EXPECT_EQ(0, health_checker_->num_connection_attempts_);
  }
  // Setup ConnectionHealthChecker::GetSocketInfo to return sock_info.
  // This only works if GetSocketInfo is called with kProxyFD.
  // If no matching sock_info is provided (Does not belong to proxy socket),
  // GetSocketInfo will (correctly) return false.
  void ExpectGetSocketInfoReturns(SocketInfo sock_info) {
    vector<SocketInfo> info_list;
    info_list.push_back(sock_info);
    EXPECT_CALL(*socket_, GetSockName(kProxyFD, _, _))
        .InSequence(seq_)
        .WillOnce(Invoke(this,
                         &ConnectionHealthCheckerTest::GetSockName));
    EXPECT_CALL(*socket_info_reader_, LoadTcpSocketInfo(_))
        .InSequence(seq_)
        .WillOnce(DoAll(SetArgumentPointee<0>(info_list),
                        Return(true)));

  }
  void ExpectSuccessfulStart() {
    EXPECT_CALL(
        *tcp_connection_,
        Start(IsSameIPAddress(StringToIPv4Address(kProxyIPAddressRemote)),
              kProxyPortRemote))
        .InSequence(seq_)
        .WillOnce(Return(true));
  }
  void ExpectRetry() {
    EXPECT_CALL(*socket_, Close(kProxyFD))
        .Times(1)
        .InSequence(seq_);
    EXPECT_CALL(
        *tcp_connection_,
        Start(IsSameIPAddress(StringToIPv4Address(kProxyIPAddressRemote)),
              kProxyPortRemote))
        .InSequence(seq_)
        .WillOnce(Return(true));
  }
  void ExpectSendDataReturns(ConnectionHealthChecker::Result result) {
    // Turn on SendData
    health_checker_->set_run_data_test(true);
    // These scenarios are copied from the SendData unit-test, and must match
    // those in the test.
    switch(result) {
      case ConnectionHealthChecker::kResultSuccess:
        ExpectGetSocketInfoReturns(
            CreateSocketInfoProxy(
                SocketInfo::kConnectionStateEstablished, 0));
        EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
            .InSequence(seq_)
            .WillOnce(Return(0));
        ExpectGetSocketInfoReturns(
            CreateSocketInfoProxy(
                SocketInfo::kConnectionStateEstablished, 0));
        break;
      case ConnectionHealthChecker::kResultConnectionFailure:
        ExpectGetSocketInfoReturns(
            CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished));
        EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
            .InSequence(seq_)
            .WillOnce(Return(-1));
        EXPECT_CALL(*socket_, Error())
            .InSequence(seq_)
            .WillOnce(Return(0));
        break;
      case ConnectionHealthChecker::kResultUnknown:
        ExpectGetSocketInfoReturns(CreateSocketInfoOther());
        break;
      case ConnectionHealthChecker::kResultCongestedTxQueue:
        ExpectGetSocketInfoReturns(
            CreateSocketInfoProxy(
                SocketInfo::kConnectionStateEstablished, 1));
        EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
            .InSequence(seq_)
            .WillOnce(Return(0));
        ExpectGetSocketInfoReturns(
            CreateSocketInfoProxy(
                SocketInfo::kConnectionStateEstablished, 2));
        break;
      default:
        LOG(WARNING) << __func__ << "Unknown ConnectionHealthChecker::Result";
    }
  }
  void ExpectShutDownReturns(ConnectionHealthChecker::Result result) {
    // Turn on ShutDown
    health_checker_->set_run_data_test(false);
    if (result != ConnectionHealthChecker::kResultElongatedTimeWait) {
      LOG(WARNING) << __func__ << ": Only implements expectation for "
                   << "kResultElongatedTimeWait.";
      return;
    }
    EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
        .InSequence(seq_)
        .WillOnce(Return(0));
    ExpectGetSocketInfoReturns(
        CreateSocketInfoProxy(SocketInfo::kConnectionStateFinWait1));
  }
  void ExpectCleanUp() {
    EXPECT_CALL(*socket_, Close(kProxyFD))
        .Times(1)
        .InSequence(seq_);
    EXPECT_CALL(*tcp_connection_, Stop())
        .Times(1)
        .InSequence(seq_);
  }

  // Needed for other mocks, but not for the tests directly.
  const string interface_name_;
  NiceMock<MockControl> control_;
  NiceMock<MockDeviceInfo> device_info_;
  vector<string> dns_servers_;

  scoped_refptr<MockConnection> connection_;
  StrictMock<MockEventDispatcher> dispatcher_;
  StrictMock<MockSockets> *socket_;
  StrictMock<MockSocketInfoReader> *socket_info_reader_;
  StrictMock<MockAsyncConnection> *tcp_connection_;
  MockDNSClient *dns_client_;
  // Exepctations inthe Expect* functions are put in this sequence.
  // This allows us to chain calls to Expect* functions.
  Sequence seq_;

  scoped_ptr<ConnectionHealthChecker> health_checker_;
};

TEST_F(ConnectionHealthCheckerTest, Constructor) {
  ExpectReset();
}

TEST_F(ConnectionHealthCheckerTest, AddRemoteIP) {
  EXPECT_EQ(0, remote_ips().size());

  IPAddress ip = StringToIPv4Address(kIPAddress_0_0_0_0);
  health_checker_->AddRemoteIP(ip);
  EXPECT_EQ(1, remote_ips().size());
  EXPECT_TRUE(remote_ips().front().Equals(ip));

  health_checker_->AddRemoteIP(
      StringToIPv4Address(kIPAddress_0_0_0_0));
  EXPECT_EQ(2, remote_ips().size());
}

TEST_F(ConnectionHealthCheckerTest, AddRemoteURL) {
  ConnectionHealthChecker::IPAddressQueue::size_type num_old_ips;

  // DNS query fails synchronously.
  EXPECT_CALL(*dns_client_, Start(_,_))
      .WillOnce(Return(false));
  num_old_ips = remote_ips().size();
  health_checker_->AddRemoteURL(kProxyURLRemote);
  EXPECT_EQ(num_old_ips, remote_ips().size());
  Mock::VerifyAndClearExpectations(dns_client_);

  // DNS query fails asynchronously.
  EXPECT_CALL(*dns_client_, Start(_,_))
      .WillOnce(Return(true));
  num_old_ips = remote_ips().size();
  health_checker_->AddRemoteURL(kProxyURLRemote);
  GetDNSResultFailure();
  EXPECT_EQ(num_old_ips, remote_ips().size());
  Mock::VerifyAndClearExpectations(dns_client_);

  // Success
  EXPECT_CALL(*dns_client_, Start(_,_))
      .WillOnce(Return(true));
  num_old_ips = remote_ips().size();
  health_checker_->AddRemoteURL(kProxyURLRemote);
  IPAddress remote_ip = StringToIPv4Address(kProxyIPAddressRemote);
  GetDNSResultSuccess(remote_ip);
  EXPECT_EQ(num_old_ips + 1, remote_ips().size());
  EXPECT_TRUE(remote_ip.Equals(remote_ips().front()));
  Mock::VerifyAndClearExpectations(dns_client_);
}

TEST_F(ConnectionHealthCheckerTest, GetSocketInfo) {
  SocketInfo sock_info;
  vector<SocketInfo> info_list;

  // GetSockName fails.
  EXPECT_CALL(*socket_, GetSockName(_,_,_))
      .WillOnce(Return(-1));
  EXPECT_FALSE(health_checker_->GetSocketInfo(kProxyFD, &sock_info));
  Mock::VerifyAndClearExpectations(socket_);

  // GetSockName returns IPv6.
  EXPECT_CALL(*socket_, GetSockName(_,_,_))
      .WillOnce(
          Invoke(this,
                 &ConnectionHealthCheckerTest::GetSockNameReturnsIPv6));
  EXPECT_FALSE(health_checker_->GetSocketInfo(kProxyFD, &sock_info));
  Mock::VerifyAndClearExpectations(socket_);

  // LoadTcpSocketInfo fails.
  EXPECT_CALL(*socket_, GetSockName(kProxyFD, _, _))
      .WillOnce(Invoke(this, &ConnectionHealthCheckerTest::GetSockName));
  EXPECT_CALL(*socket_info_reader_, LoadTcpSocketInfo(_))
    .WillOnce(Return(false));
  EXPECT_FALSE(health_checker_->GetSocketInfo(kProxyFD, &sock_info));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // LoadTcpSocketInfo returns empty list.
  info_list.clear();
  EXPECT_CALL(*socket_, GetSockName(kProxyFD, _, _))
      .WillOnce(Invoke(this, &ConnectionHealthCheckerTest::GetSockName));
  EXPECT_CALL(*socket_info_reader_, LoadTcpSocketInfo(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(info_list),
                      Return(true)));
  EXPECT_FALSE(health_checker_->GetSocketInfo(kProxyFD, &sock_info));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // LoadTcpSocketInfo returns a list without our socket.
  info_list.clear();
  info_list.push_back(CreateSocketInfoOther());
  EXPECT_CALL(*socket_, GetSockName(kProxyFD, _, _))
      .WillOnce(Invoke(this, &ConnectionHealthCheckerTest::GetSockName));
  EXPECT_CALL(*socket_info_reader_, LoadTcpSocketInfo(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(info_list),
                      Return(true)));
  EXPECT_FALSE(health_checker_->GetSocketInfo(kProxyFD, &sock_info));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // LoadTcpSocketInfo returns a list with only our socket.
  info_list.clear();
  info_list.push_back(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateUnknown));
  EXPECT_CALL(*socket_, GetSockName(kProxyFD, _, _))
      .WillOnce(Invoke(this, &ConnectionHealthCheckerTest::GetSockName));
  EXPECT_CALL(*socket_info_reader_, LoadTcpSocketInfo(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(info_list),
                      Return(true)));
  EXPECT_TRUE(health_checker_->GetSocketInfo(kProxyFD, &sock_info));
  EXPECT_TRUE(CreateSocketInfoProxy(SocketInfo::kConnectionStateUnknown)
                  .IsSameSocketAs(sock_info));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // LoadTcpSocketInfo returns a list with two sockets, including ours.
  info_list.clear();
  info_list.push_back(CreateSocketInfoOther());
  info_list.push_back(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateUnknown));
  EXPECT_CALL(*socket_, GetSockName(kProxyFD, _, _))
      .WillOnce(Invoke(this, &ConnectionHealthCheckerTest::GetSockName));
  EXPECT_CALL(*socket_info_reader_, LoadTcpSocketInfo(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(info_list),
                      Return(true)));
  EXPECT_TRUE(health_checker_->GetSocketInfo(kProxyFD, &sock_info));
  EXPECT_TRUE(CreateSocketInfoProxy(SocketInfo::kConnectionStateUnknown)
                  .IsSameSocketAs(sock_info));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  info_list.clear();
  info_list.push_back(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateUnknown));
  info_list.push_back(CreateSocketInfoOther());
  EXPECT_CALL(*socket_, GetSockName(kProxyFD, _, _))
      .WillOnce(Invoke(this, &ConnectionHealthCheckerTest::GetSockName));
  EXPECT_CALL(*socket_info_reader_, LoadTcpSocketInfo(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(info_list),
                      Return(true)));
  EXPECT_TRUE(health_checker_->GetSocketInfo(kProxyFD, &sock_info));
  EXPECT_TRUE(CreateSocketInfoProxy(SocketInfo::kConnectionStateUnknown)
                  .IsSameSocketAs(sock_info));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);
}

TEST_F(ConnectionHealthCheckerTest, ShutDown) {
  // Sockets::ShutDown fails.
  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(-1));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);

  // SocketInfo in different connection states.
  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateSynSent));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateSynRecv));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateFinWait1));
  EXPECT_EQ(ConnectionHealthChecker::kResultElongatedTimeWait,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateFinWait2));
  EXPECT_EQ(ConnectionHealthChecker::kResultElongatedTimeWait,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateTimeWait));
  EXPECT_EQ(ConnectionHealthChecker::kResultElongatedTimeWait,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateClose));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateCloseWait));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateLastAck));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateListen));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateClosing));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->ShutDown(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // ConnectionHealthChecker::GetSocketInfo returns false.
  // Since there is no entry in /proc, the shutdown must have been
  // successful.
  // TODO(pprabhu) Verify this can't happen: We check /proc *before* the
  // established connection is even put there.
  EXPECT_CALL(*socket_, ShutDown(kProxyFD, _))
      .WillOnce(Return(0));
  ExpectGetSocketInfoReturns(CreateSocketInfoOther());
  EXPECT_EQ(ConnectionHealthChecker::kResultSuccess,
            health_checker_->ShutDown(kProxyFD));
}

TEST_F(ConnectionHealthCheckerTest, SendData) {
  // Connection doesn't exist.
  ExpectGetSocketInfoReturns(CreateSocketInfoOther());
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // Connection in state other than Established
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateTimeWait));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // Connection established, send fails.
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished));
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(-1));
  EXPECT_CALL(*socket_, Error())
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultConnectionFailure,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // Connection established, send succeeds, and
  // Connection dissapears
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished));
  ExpectGetSocketInfoReturns(CreateSocketInfoOther());
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // Connection switches to bad state
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateClose));
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultUnknown,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // Connection remains good, data not sent
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 1));
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultCongestedTxQueue,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 1));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 2));
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultCongestedTxQueue,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 10));
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultCongestedTxQueue,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  // Connection remains good, data sent.
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 0));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 0));
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultSuccess,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 3));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 3));
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultSuccess,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);

  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 10));
  ExpectGetSocketInfoReturns(
      CreateSocketInfoProxy(SocketInfo::kConnectionStateEstablished, 0));
  EXPECT_CALL(*socket_, Send(kProxyFD, _,Gt(0),_))
      .WillOnce(Return(0));
  EXPECT_EQ(ConnectionHealthChecker::kResultSuccess,
            health_checker_->SendData(kProxyFD));
  Mock::VerifyAndClearExpectations(socket_);
  Mock::VerifyAndClearExpectations(socket_info_reader_);
}

// Scenario tests.
// Flow: Start() -> Start()
// Expectation: Only one AsyncConnection is setup
TEST_F(ConnectionHealthCheckerTest, StartStartSkipsSecond) {
  EXPECT_CALL(*tcp_connection_, Start(_,_))
      .WillOnce(Return(true));
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  health_checker_->Start();
}

// Precondition: size(|remote_ips_|) > 0
// Flow: Start() -> Stop() before ConnectionComplete()
// Expectation: No call to |result_callback|
TEST_F(ConnectionHealthCheckerTest, StartStopNoCallback) {
  EXPECT_CALL(*tcp_connection_, Start(_,_))
      .WillOnce(Return(true));
  EXPECT_CALL(*tcp_connection_, Stop())
      .Times(1);
  EXPECT_CALL(*this, ResultCallbackTarget(_))
      .Times(0);
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  health_checker_->Stop();
}

// Precondition: Empty remote_ips_
// Flow: Start()
// Expectation: call |result_callback| with kResultUnknown
// Precondition: Non-empty remote_ips_
// Flow: Start() -> synchronous async_connection failure
// Expectation: call |result_callback| with kResultConnectionFailure
TEST_F(ConnectionHealthCheckerTest, StartImmediateFailure) {
  EXPECT_CALL(*tcp_connection_, Stop())
      .Times(1);
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultUnknown))
      .Times(1);
  health_checker_->Start();
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(tcp_connection_);

  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  EXPECT_CALL(*tcp_connection_,
              Start(IsSameIPAddress(StringToIPv4Address(kProxyIPAddressRemote)),
                    kProxyPortRemote))
      .WillOnce(Return(false));
  EXPECT_CALL(*tcp_connection_, Stop())
      .Times(1);
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultConnectionFailure))
      .Times(1);
  health_checker_->Start();
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(tcp_connection_);
}

// Precondition: len(|remote_ips_|) == 1
// Flow: Start() -> asynchronous async_connection failure
// Expectation: call |result_callback| with kResultConnectionFailure
TEST_F(ConnectionHealthCheckerTest, StartAsynchrnousFailure) {
  EXPECT_CALL(*tcp_connection_,
              Start(IsSameIPAddress(StringToIPv4Address(kProxyIPAddressRemote)),
                    kProxyPortRemote))
      .WillOnce(Return(true));
  EXPECT_CALL(*tcp_connection_, Stop())
      .Times(1);
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultConnectionFailure))
      .Times(1);
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  InvokeOnConnectionComplete(false, -1);  // second argument ignored.
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(tcp_connection_);
}

// Precondition: len(|remote_ips_|) == 1
// Flow: Start() -> Connection Successful -> SendData() returns kResultSucess
// Expectation: call |result_callback| with kResultSuccess
TEST_F(ConnectionHealthCheckerTest, HealthCheckSuccess) {
  ExpectSuccessfulStart();
  ExpectSendDataReturns(ConnectionHealthChecker::kResultSuccess);
  ExpectCleanUp();
  // Expectation:
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultSuccess))
      .Times(1);
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  InvokeOnConnectionComplete(true, kProxyFD);
  VerifyAndClearAllExpectations();
}

// Precondition: len(|remote_ips_|) == 1
// Flow: Start() -> Connection Successful
//       -> SendData() returns kResultCongestedTxQueue
// Expectation: call |result_callback| with kResultCongestedTxQueue
TEST_F(ConnectionHealthCheckerTest, HealthCheckCongestedQueue) {
  ExpectSuccessfulStart();
  ExpectSendDataReturns(ConnectionHealthChecker::kResultCongestedTxQueue);
  ExpectCleanUp();
  // Expectation:
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultCongestedTxQueue))
      .Times(1);
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  InvokeOnConnectionComplete(true, kProxyFD);
  VerifyAndClearAllExpectations();
}

// Precondition: len(|remote_ips_|) == 1
// Flow: Start() -> Connection Successful
//       -> ShutDown() returns kResultElongatedTimeWait
// Expectation: call |result_callback| with kResultElongatedTimeWait
TEST_F(ConnectionHealthCheckerTest, HealthCheckElongatedTimeWait) {
  ExpectSuccessfulStart();
  ExpectShutDownReturns(ConnectionHealthChecker::kResultElongatedTimeWait);
  ExpectCleanUp();
  // Expectation:
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultElongatedTimeWait))
      .Times(1);
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  InvokeOnConnectionComplete(true, kProxyFD);
  VerifyAndClearAllExpectations();
}

// Precondition: len(|remote_ips_|) == 2
// Flow: Start() -> Connection Successful ->
//       SendData() returns kResultUnknown ->
//       SendData() returns kResultUnknown
// Expectation: (1) call |result_callback| with kResultConnectionFailure
//              (2) Sockets::Close called twice
TEST_F(ConnectionHealthCheckerTest, HealthCheckFailOutOfIPs) {
  ExpectSuccessfulStart();
  ExpectSendDataReturns(ConnectionHealthChecker::kResultUnknown);
  ExpectRetry();
  ExpectSendDataReturns(ConnectionHealthChecker::kResultUnknown);
  ExpectCleanUp();
  // Expectation:
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultConnectionFailure))
      .Times(1);
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  InvokeOnConnectionComplete(true, kProxyFD);
  InvokeOnConnectionComplete(true, kProxyFD);
  VerifyAndClearAllExpectations();
}

// Precondition: len(|remote_ips_|) == 2
// Flow: Start() -> Connection Successful -> SendData() returns kResultUnknown
//       -> SendData() returns kResultSuccess
// Expectation: call |result_callback| with kResultSuccess
//              (2) Sockets::Close called twice
TEST_F(ConnectionHealthCheckerTest, HealthCheckSuccessSecondIP) {
  ExpectSuccessfulStart();
  ExpectSendDataReturns(ConnectionHealthChecker::kResultUnknown);
  ExpectRetry();
  ExpectSendDataReturns(ConnectionHealthChecker::kResultSuccess);
  ExpectCleanUp();
  // Expectation:
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultSuccess))
      .Times(1);
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  InvokeOnConnectionComplete(true, kProxyFD);
  InvokeOnConnectionComplete(true, kProxyFD);
  VerifyAndClearAllExpectations();
}

//
// Precondition: len(|remote_ips_|) > |kMaxConnectionAttempts|
// Flow: Start() -> Connection Successful
//               -> Forever(SendData() returns kResultUnknown)
// Expectation: call |result_callback| with kResultConnectionFailure
//              (2) Sockets::Close called |kMaxConnectionAttempts| times
//
TEST_F(ConnectionHealthCheckerTest, HealthCheckFailHitMaxConnectionAttempts) {
  ExpectSuccessfulStart();
  ExpectSendDataReturns(ConnectionHealthChecker::kResultUnknown);
  for (int i = 0; i < MaxConnectionAttempts()-1; ++i) {
    ExpectRetry();
    ExpectSendDataReturns(ConnectionHealthChecker::kResultUnknown);
  }
  ExpectCleanUp();
  // Expectation:
  EXPECT_CALL(*this, ResultCallbackTarget(
                           ConnectionHealthChecker::kResultConnectionFailure))
      .Times(1);
  for (int i = 0; i < MaxConnectionAttempts() + 2; ++i)
    health_checker_->AddRemoteIP(StringToIPv4Address(kProxyIPAddressRemote));
  health_checker_->Start();
  for (int i = 0; i < MaxConnectionAttempts(); ++i)
    InvokeOnConnectionComplete(true, kProxyFD);
  VerifyAndClearAllExpectations();
}

}  // namespace shill
