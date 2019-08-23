// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection_info.h"

#include <netinet/in.h>

#include <gtest/gtest.h>

namespace shill {

namespace {

const unsigned char kIPAddress1[] = {192, 168, 1, 1};
const unsigned char kIPAddress2[] = {192, 168, 1, 2};
const unsigned char kIPAddress3[] = {192, 168, 1, 3};
const unsigned char kIPAddress4[] = {192, 168, 1, 4};
const uint16_t kPort1 = 1000;
const uint16_t kPort2 = 2000;
const uint16_t kPort3 = 3000;
const uint16_t kPort4 = 4000;

}  // namespace

class ConnectionInfoTest : public testing::Test {
 protected:
  void ExpectConnectionInfoEqual(const ConnectionInfo& info1,
                                 const ConnectionInfo& info2) {
    EXPECT_EQ(info1.protocol, info2.protocol);
    EXPECT_EQ(info1.time_to_expire_seconds, info2.time_to_expire_seconds);
    EXPECT_EQ(info1.is_unreplied, info2.is_unreplied);
    EXPECT_TRUE(info1.original_source_ip_address.Equals(
        info2.original_source_ip_address));
    EXPECT_EQ(info1.original_source_port, info2.original_source_port);
    EXPECT_TRUE(info1.original_destination_ip_address.Equals(
        info2.original_destination_ip_address));
    EXPECT_EQ(info1.original_destination_port, info2.original_destination_port);
    EXPECT_TRUE(
        info1.reply_source_ip_address.Equals(info2.reply_source_ip_address));
    EXPECT_EQ(info1.reply_source_port, info2.reply_source_port);
    EXPECT_TRUE(info1.reply_destination_ip_address.Equals(
        info2.reply_destination_ip_address));
    EXPECT_EQ(info1.reply_destination_port, info2.reply_destination_port);
  }
};

TEST_F(ConnectionInfoTest, CopyConstructor) {
  ConnectionInfo info(IPPROTO_UDP, 10, true,
                      IPAddress(IPAddress::kFamilyIPv4,
                                ByteString(kIPAddress1, sizeof(kIPAddress1))),
                      kPort1,
                      IPAddress(IPAddress::kFamilyIPv4,
                                ByteString(kIPAddress2, sizeof(kIPAddress2))),
                      kPort2,
                      IPAddress(IPAddress::kFamilyIPv4,
                                ByteString(kIPAddress3, sizeof(kIPAddress3))),
                      kPort3,
                      IPAddress(IPAddress::kFamilyIPv4,
                                ByteString(kIPAddress4, sizeof(kIPAddress4))),
                      kPort4);

  ConnectionInfo info_copy(info);
  ExpectConnectionInfoEqual(info, info_copy);
}

TEST_F(ConnectionInfoTest, AssignmentOperator) {
  ConnectionInfo info(IPPROTO_UDP, 10, true,
                      IPAddress(IPAddress::kFamilyIPv4,
                                ByteString(kIPAddress1, sizeof(kIPAddress1))),
                      kPort1,
                      IPAddress(IPAddress::kFamilyIPv4,
                                ByteString(kIPAddress2, sizeof(kIPAddress2))),
                      kPort2,
                      IPAddress(IPAddress::kFamilyIPv4,
                                ByteString(kIPAddress3, sizeof(kIPAddress3))),
                      kPort3,
                      IPAddress(IPAddress::kFamilyIPv4,
                                ByteString(kIPAddress4, sizeof(kIPAddress4))),
                      kPort4);

  ConnectionInfo info_copy = info;
  ExpectConnectionInfoEqual(info, info_copy);
}

}  // namespace shill
