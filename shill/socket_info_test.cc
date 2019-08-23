// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/socket_info.h"

#include <gtest/gtest.h>

namespace shill {

namespace {

const unsigned char kIPAddress1[] = {192, 168, 1, 1};
const unsigned char kIPAddress2[] = {192, 168, 1, 2};
const uint16_t kPort1 = 1000;
const uint16_t kPort2 = 2000;

}  // namespace

class SocketInfoTest : public testing::Test {
 protected:
  void ExpectSocketInfoEqual(const SocketInfo& info1, const SocketInfo& info2) {
    EXPECT_EQ(info1.connection_state, info2.connection_state);
    EXPECT_TRUE(info1.local_ip_address.Equals(info2.local_ip_address));
    EXPECT_EQ(info1.local_port, info2.local_port);
    EXPECT_TRUE(info1.remote_ip_address.Equals(info2.remote_ip_address));
    EXPECT_EQ(info1.remote_port, info2.remote_port);
    EXPECT_EQ(info1.transmit_queue_value, info2.transmit_queue_value);
    EXPECT_EQ(info1.receive_queue_value, info2.receive_queue_value);
    EXPECT_EQ(info1.timer_state, info2.timer_state);
  }
};

TEST_F(SocketInfoTest, CopyConstructor) {
  SocketInfo info(SocketInfo::kConnectionStateEstablished,
                  IPAddress(IPAddress::kFamilyIPv4,
                            ByteString(kIPAddress1, sizeof(kIPAddress1))),
                  kPort1,
                  IPAddress(IPAddress::kFamilyIPv4,
                            ByteString(kIPAddress2, sizeof(kIPAddress2))),
                  kPort2, 10, 20,
                  SocketInfo::kTimerStateRetransmitTimerPending);

  SocketInfo info_copy(info);
  ExpectSocketInfoEqual(info, info_copy);
}

TEST_F(SocketInfoTest, AssignmentOperator) {
  SocketInfo info(SocketInfo::kConnectionStateEstablished,
                  IPAddress(IPAddress::kFamilyIPv4,
                            ByteString(kIPAddress1, sizeof(kIPAddress1))),
                  kPort1,
                  IPAddress(IPAddress::kFamilyIPv4,
                            ByteString(kIPAddress2, sizeof(kIPAddress2))),
                  kPort2, 10, 20,
                  SocketInfo::kTimerStateRetransmitTimerPending);

  SocketInfo info_copy = info;
  ExpectSocketInfoEqual(info, info_copy);
}

}  // namespace shill
