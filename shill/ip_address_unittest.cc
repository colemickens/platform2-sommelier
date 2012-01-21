// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include "shill/byte_string.h"
#include "shill/ip_address.h"

using std::string;
using testing::Test;

namespace shill {

namespace {
const char kV4String1[] = "192.168.10.1";
const unsigned char kV4Address1[] = { 192, 168, 10, 1 };
const char kV4String2[] = "192.168.10";
const unsigned char kV4Address2[] = { 192, 168, 10 };
const char kV6String1[] = "fe80::1aa9:5ff:7ebf:14c5";
const unsigned char kV6Address1[] = { 0xfe, 0x80, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00,
                                      0x1a, 0xa9, 0x05, 0xff,
                                      0x7e, 0xbf, 0x14, 0xc5 };
const char kV6String2[] = "1980:0:1000:1b02:1aa9:5ff:7ebf";
const unsigned char kV6Address2[] = { 0x19, 0x80, 0x00, 0x00,
                                      0x10, 0x00, 0x1b, 0x02,
                                      0x1a, 0xa9, 0x05, 0xff,
                                      0x7e, 0xbf };
}  // namespace {}

class IPAddressTest : public Test {
 protected:
  void TestAddress(IPAddress::Family family,
                   const string &good_string,
                   const ByteString &good_bytes,
                   const string &bad_string,
                   const ByteString &bad_bytes) {
    IPAddress good_addr(family);

    EXPECT_TRUE(good_addr.SetAddressFromString(good_string));
    EXPECT_EQ(IPAddress::GetAddressLength(family), good_addr.GetLength());
    EXPECT_EQ(family, good_addr.family());
    EXPECT_FALSE(good_addr.IsDefault());
    EXPECT_EQ(0, memcmp(good_addr.GetConstData(), good_bytes.GetConstData(),
                        good_bytes.GetLength()));
    EXPECT_TRUE(good_addr.address().Equals(good_bytes));
    string address_string;
    EXPECT_TRUE(good_addr.ToString(&address_string));
    EXPECT_EQ(good_string, address_string);

    IPAddress good_addr_from_bytes(family, good_bytes);
    EXPECT_TRUE(good_addr.Equals(good_addr_from_bytes));

    IPAddress bad_addr(family);
    EXPECT_FALSE(bad_addr.SetAddressFromString(bad_string));
    EXPECT_FALSE(good_addr.Equals(bad_addr));

    EXPECT_FALSE(bad_addr.IsValid());

    IPAddress bad_addr_from_bytes(family, bad_bytes);
    EXPECT_EQ(family, bad_addr_from_bytes.family());
    EXPECT_FALSE(bad_addr_from_bytes.IsValid());

    EXPECT_FALSE(bad_addr.Equals(bad_addr_from_bytes));
    EXPECT_FALSE(bad_addr.ToString(&address_string));
  }
};

TEST_F(IPAddressTest, Statics) {
  EXPECT_EQ(4, IPAddress::GetAddressLength(IPAddress::kFamilyIPv4));
  EXPECT_EQ(16, IPAddress::GetAddressLength(IPAddress::kFamilyIPv6));

  IPAddress addr4(IPAddress::kFamilyIPv4);
  addr4.SetAddressToDefault();

  EXPECT_EQ(4, addr4.GetLength());
  EXPECT_EQ(IPAddress::kFamilyIPv4, addr4.family());
  EXPECT_TRUE(addr4.IsDefault());
  EXPECT_TRUE(addr4.address().IsZero());
  EXPECT_TRUE(addr4.address().Equals(ByteString(4)));


  IPAddress addr6(IPAddress::kFamilyIPv6);
  addr6.SetAddressToDefault();

  EXPECT_EQ(16, addr6.GetLength());
  EXPECT_EQ(addr6.family(), IPAddress::kFamilyIPv6);
  EXPECT_TRUE(addr6.IsDefault());
  EXPECT_TRUE(addr6.address().IsZero());
  EXPECT_TRUE(addr6.address().Equals(ByteString(16)));

  EXPECT_FALSE(addr4.Equals(addr6));
}

TEST_F(IPAddressTest, IPv4) {
  TestAddress(IPAddress::kFamilyIPv4,
              kV4String1, ByteString(kV4Address1, sizeof(kV4Address1)),
              kV4String2, ByteString(kV4Address2, sizeof(kV4Address2)));
}


TEST_F(IPAddressTest, IPv6) {
  TestAddress(IPAddress::kFamilyIPv6,
              kV6String1, ByteString(kV6Address1, sizeof(kV6Address1)),
              kV6String2, ByteString(kV6Address2, sizeof(kV6Address2)));
}

}  // namespace shill
