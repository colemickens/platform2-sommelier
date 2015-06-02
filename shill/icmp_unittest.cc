// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/icmp.h"

#include <netinet/in.h>
#include <netinet/ip_icmp.h>

#include <gtest/gtest.h>

#include "shill/mock_log.h"
#include "shill/net/ip_address.h"
#include "shill/net/mock_sockets.h"

using testing::_;
using testing::HasSubstr;
using testing::InSequence;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

class IcmpTest : public Test {
 public:
  IcmpTest() {}
  virtual ~IcmpTest() {}

  virtual void SetUp() {
    sockets_ = new StrictMock<MockSockets>();
    // Passes ownership.
    icmp_.sockets_.reset(sockets_);
  }

  virtual void TearDown() {
    if (icmp_.IsStarted()) {
      EXPECT_CALL(*sockets_, Close(kSocketFD));
      icmp_.Stop();
    }
    EXPECT_FALSE(icmp_.IsStarted());
  }

 protected:
  static const int kSocketFD;
  static const char kIPAddress[];

  int GetSocket() { return icmp_.socket_; }
  bool StartIcmp() { return StartIcmpWithFD(kSocketFD); }
  bool StartIcmpWithFD(int fd) {
    EXPECT_CALL(*sockets_, Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP))
        .WillOnce(Return(fd));
    EXPECT_CALL(*sockets_, SetNonBlocking(fd)).WillOnce(Return(0));
    bool start_status = icmp_.Start();
    EXPECT_TRUE(start_status);
    EXPECT_EQ(fd, icmp_.socket_);
    EXPECT_TRUE(icmp_.IsStarted());
    return start_status;
  }

  // Owned by Icmp, and tracked here only for mocks.
  MockSockets *sockets_;

  Icmp icmp_;
};


const int IcmpTest::kSocketFD = 456;
const char IcmpTest::kIPAddress[] = "10.0.1.1";


TEST_F(IcmpTest, Constructor) {
  EXPECT_EQ(-1, GetSocket());
  EXPECT_FALSE(icmp_.IsStarted());
}

TEST_F(IcmpTest, SocketOpenFail) {
  ScopedMockLog log;
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("Could not create ICMP socket"))).Times(1);

  EXPECT_CALL(*sockets_, Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP))
      .WillOnce(Return(-1));
  EXPECT_FALSE(icmp_.Start());
  EXPECT_FALSE(icmp_.IsStarted());
}

TEST_F(IcmpTest, SocketNonBlockingFail) {
  ScopedMockLog log;
  EXPECT_CALL(log,
      Log(logging::LOG_ERROR, _,
          HasSubstr("Could not set socket to be non-blocking"))).Times(1);

  EXPECT_CALL(*sockets_, Socket(_, _, _)).WillOnce(Return(kSocketFD));
  EXPECT_CALL(*sockets_, SetNonBlocking(kSocketFD)).WillOnce(Return(-1));
  EXPECT_CALL(*sockets_, Close(kSocketFD));
  EXPECT_FALSE(icmp_.Start());
  EXPECT_FALSE(icmp_.IsStarted());
}

TEST_F(IcmpTest, StartMultipleTimes) {
  const int kFirstSocketFD = kSocketFD + 1;
  StartIcmpWithFD(kFirstSocketFD);
  EXPECT_CALL(*sockets_, Close(kFirstSocketFD));
  StartIcmp();
}

MATCHER_P(IsIcmpHeader, header, "") {
  return memcmp(arg, &header, sizeof(header)) == 0;
}


MATCHER_P(IsSocketAddress, address, "") {
  const struct sockaddr_in *sock_addr =
      reinterpret_cast<const struct sockaddr_in *>(arg);
  return sock_addr->sin_family == address.family() &&
      memcmp(&sock_addr->sin_addr.s_addr, address.GetConstData(),
             address.GetLength()) == 0;
}

TEST_F(IcmpTest, TransmitEchoRequest) {
  StartIcmp();
  // Address isn't valid.
  EXPECT_FALSE(icmp_.TransmitEchoRequest(IPAddress(IPAddress::kFamilyIPv4)));

  // IPv6 adresses aren't implemented.
  IPAddress ipv6_destination(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(ipv6_destination.SetAddressFromString(
      "fe80::1aa9:5ff:abcd:1234"));
  EXPECT_FALSE(icmp_.TransmitEchoRequest(ipv6_destination));

  IPAddress ipv4_destination(IPAddress::kFamilyIPv4);
  EXPECT_TRUE(ipv4_destination.SetAddressFromString(kIPAddress));

  struct icmphdr icmp_header;
  memset(&icmp_header, 0, sizeof(icmp_header));
  icmp_header.type = ICMP_ECHO;
  icmp_header.un.echo.id = 1;
  icmp_header.un.echo.sequence = 1;
  EXPECT_CALL(*sockets_, SendTo(kSocketFD,
                                IsIcmpHeader(icmp_header),
                                sizeof(icmp_header),
                                0,
                                IsSocketAddress(ipv4_destination),
                                sizeof(sockaddr_in)))
      .WillOnce(Return(-1))
      .WillOnce(Return(0))
      .WillOnce(Return(sizeof(icmp_header) - 1))
      .WillOnce(Return(sizeof(icmp_header)));
  {
    InSequence seq;
    ScopedMockLog log;
    EXPECT_CALL(log,
        Log(logging::LOG_ERROR, _,
            HasSubstr("Socket sendto failed"))).Times(1);
    EXPECT_CALL(log,
        Log(logging::LOG_ERROR, _,
            HasSubstr("less than the expected result"))).Times(2);

    EXPECT_FALSE(icmp_.TransmitEchoRequest(ipv4_destination));
    EXPECT_FALSE(icmp_.TransmitEchoRequest(ipv4_destination));
    EXPECT_FALSE(icmp_.TransmitEchoRequest(ipv4_destination));
    EXPECT_TRUE(icmp_.TransmitEchoRequest(ipv4_destination));
  }
}

}  // namespace shill
