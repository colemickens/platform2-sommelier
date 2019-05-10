// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ip_address_store.h"

#include <string>

#include <gtest/gtest.h>

using ::std::string;
using ::testing::Test;

namespace shill {

namespace {
const char kIPAddress_0_0_0_0[] = "0.0.0.0";
const char kIPAddress_8_8_8_8[] = "8.8.8.8";
const char kIPAddress_7_7_7_7[] = "7.7.7.7";
}  // namespace

class IPAddressStoreTest : public Test {
 public:
  IPAddressStoreTest() {}

 protected:
  IPAddress StringToIPv4Address(const string& address_string) {
    IPAddress ip_address(IPAddress::kFamilyIPv4);
    EXPECT_TRUE(ip_address.SetAddressFromString(address_string));
    return ip_address;
  }

  void PopulateIPAddressStore() {
    ip_address_store_.AddUnique(StringToIPv4Address(kIPAddress_0_0_0_0));
    ip_address_store_.AddUnique(StringToIPv4Address(kIPAddress_8_8_8_8));
    ip_address_store_.AddUnique(StringToIPv4Address(kIPAddress_7_7_7_7));
  }

  IPAddressStore ip_address_store_;
};

TEST_F(IPAddressStoreTest, AddUnique) {
  IPAddress ip_0_0_0_0 = StringToIPv4Address(kIPAddress_0_0_0_0);
  IPAddress ip_8_8_8_8 = StringToIPv4Address(kIPAddress_8_8_8_8);
  IPAddress ip_7_7_7_7 = StringToIPv4Address(kIPAddress_7_7_7_7);

  EXPECT_EQ(0, ip_address_store_.Count());
  ip_address_store_.AddUnique(ip_0_0_0_0);
  EXPECT_TRUE(ip_address_store_.Contains(ip_0_0_0_0));
  EXPECT_FALSE(ip_address_store_.Contains(ip_8_8_8_8));
  EXPECT_FALSE(ip_address_store_.Contains(ip_7_7_7_7));
  EXPECT_EQ(1, ip_address_store_.Count());
  ip_address_store_.AddUnique(ip_8_8_8_8);
  EXPECT_TRUE(ip_address_store_.Contains(ip_8_8_8_8));
  EXPECT_FALSE(ip_address_store_.Contains(ip_7_7_7_7));
  EXPECT_EQ(2, ip_address_store_.Count());
  ip_address_store_.AddUnique(ip_8_8_8_8);
  EXPECT_TRUE(ip_address_store_.Contains(ip_0_0_0_0));
  EXPECT_TRUE(ip_address_store_.Contains(ip_8_8_8_8));
  EXPECT_EQ(2, ip_address_store_.Count());
  ip_address_store_.AddUnique(ip_0_0_0_0);
  EXPECT_EQ(2, ip_address_store_.Count());
  ip_address_store_.AddUnique(ip_7_7_7_7);
  EXPECT_TRUE(ip_address_store_.Contains(ip_7_7_7_7));
  EXPECT_EQ(3, ip_address_store_.Count());
}

}  // namespace shill
