// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/wifi_ssid_generator.h"

#include <gtest/gtest.h>

#include "libweave/src/privet/mock_delegates.h"
#include "libweave/src/privet/openssl_utils.h"

namespace weave {
namespace privet {

class WifiSsidGeneratorTest : public testing::Test {
 protected:
  void SetRandomForTests(int n) { ssid_generator_.SetRandomForTests(n); }

  testing::StrictMock<MockCloudDelegate> gcd_;
  testing::StrictMock<MockWifiDelegate> wifi_;

  WifiSsidGenerator ssid_generator_{&gcd_, &wifi_};
};

TEST_F(WifiSsidGeneratorTest, GenerateFlags) {
  EXPECT_EQ(ssid_generator_.GenerateFlags().size(), 2);

  wifi_.connection_state_ = ConnectionState{ConnectionState::kUnconfigured};
  gcd_.connection_state_ = ConnectionState{ConnectionState::kUnconfigured};
  EXPECT_EQ("DB", ssid_generator_.GenerateFlags());

  wifi_.connection_state_ = ConnectionState{ConnectionState::kOnline};
  EXPECT_EQ("CB", ssid_generator_.GenerateFlags());

  gcd_.connection_state_ = ConnectionState{ConnectionState::kOffline};
  EXPECT_EQ("AB", ssid_generator_.GenerateFlags());

  wifi_.connection_state_ = ConnectionState{ConnectionState::kUnconfigured};
  EXPECT_EQ("BB", ssid_generator_.GenerateFlags());
}

TEST_F(WifiSsidGeneratorTest, GenerateSsid31orLess) {
  EXPECT_LE(ssid_generator_.GenerateSsid().size(), 31);
}

TEST_F(WifiSsidGeneratorTest, GenerateSsidValue) {
  SetRandomForTests(47);
  EXPECT_EQ("TestDevice 47.ABMIDABprv", ssid_generator_.GenerateSsid());

  SetRandomForTests(9);
  EXPECT_EQ("TestDevice 9.ABMIDABprv", ssid_generator_.GenerateSsid());
}

TEST_F(WifiSsidGeneratorTest, GenerateSsidLongName) {
  SetRandomForTests(99);
  EXPECT_CALL(gcd_, GetName(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>("Very Long Device Name"), Return(true)));
  EXPECT_EQ("Very Long Device  99.ABMIDABprv", ssid_generator_.GenerateSsid());
}

TEST_F(WifiSsidGeneratorTest, GenerateSsidNoName) {
  SetRandomForTests(99);
  EXPECT_CALL(gcd_, GetName(_, _)).WillRepeatedly(Return(false));
  EXPECT_EQ("", ssid_generator_.GenerateSsid());
}

}  // namespace privet
}  // namespace weave
