// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/netlink_packet.h"

#include <linux/netlink.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::Test;

namespace shill {

class NetlinkPacketTest : public Test {
};

TEST_F(NetlinkPacketTest, Constructor) {
  // A null pointer should not crash the constructor, but should yield
  // an invalid packet.
  NetlinkPacket null_packet(nullptr, 100);
  EXPECT_FALSE(null_packet.IsValid());

  unsigned char data[sizeof(nlmsghdr) + 1];
  memset(&data, 0, sizeof(data));

  // A packet that is too short to contain an nlmsghdr should be invalid.
  NetlinkPacket short_packet(data, sizeof(nlmsghdr) - 1);
  EXPECT_FALSE(short_packet.IsValid());

  // A packet that contains an invalid nlmsg_len (should be at least
  // as large as sizeof(nlmgsghdr)) should be invalid.
  NetlinkPacket invalid_packet(data, sizeof(nlmsghdr));
  EXPECT_FALSE(invalid_packet.IsValid());

  // Successfully parse a well-formed packet that has no payload.
  nlmsghdr hdr;
  memset(&hdr, 0, sizeof(hdr));
  hdr.nlmsg_len = sizeof(hdr);
  hdr.nlmsg_type = 1;
  memcpy(&data, &hdr, sizeof(hdr));
  NetlinkPacket empty_packet(data, sizeof(nlmsghdr));
  EXPECT_TRUE(empty_packet.IsValid());
  EXPECT_EQ(sizeof(nlmsghdr), empty_packet.GetLength());
  EXPECT_EQ(1, empty_packet.GetMessageType());
  char payload_byte = 0;
  EXPECT_FALSE(empty_packet.ConsumeData(1, &payload_byte));

  // A packet that contains an nlmsg_len that is larger than the
  // data provided should be invalid.
  hdr.nlmsg_len = sizeof(hdr) + 1;
  hdr.nlmsg_type = 2;
  memcpy(&data, &hdr, sizeof(hdr));
  NetlinkPacket incomplete_packet(data, sizeof(nlmsghdr));
  EXPECT_FALSE(incomplete_packet.IsValid());

  // Retrieve a byte from a well-formed packet.  After that byte is
  // retrieved, no more data can be consumed.
  data[sizeof(nlmsghdr)] = 10;
  NetlinkPacket complete_packet(data, sizeof(nlmsghdr) + 1);
  EXPECT_TRUE(complete_packet.IsValid());
  EXPECT_EQ(sizeof(nlmsghdr) + 1, complete_packet.GetLength());
  EXPECT_EQ(2, complete_packet.GetMessageType());
  EXPECT_EQ(1, complete_packet.GetRemainingLength());
  EXPECT_TRUE(complete_packet.ConsumeData(1, &payload_byte));
  EXPECT_EQ(10, payload_byte);
  EXPECT_FALSE(complete_packet.ConsumeData(1, &payload_byte));
}

}  // namespace shill
