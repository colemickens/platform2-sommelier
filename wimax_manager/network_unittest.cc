// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/network.h"

#include <gtest/gtest.h>

namespace wimax_manager {

class NetworkTest : public testing::Test {
};

TEST_F(NetworkTest, GetSignalStrength) {
  static const int kExpectedStrengths[] = {
    0, 1, 2, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 22, 23, 24,
    25, 27, 28, 29, 30, 31, 33, 34, 35, 36, 37, 39, 40, 41, 42, 43, 45, 46, 47,
    48, 49, 51, 52, 53, 54, 55, 57, 58, 59, 60, 61, 63, 64, 65, 66, 67, 69, 70,
    71, 72, 73, 75, 76, 77, 78, 80, 81, 82, 83, 84, 86, 87, 88, 89, 90, 92, 93,
    94, 95, 96, 98, 99, 100
  };
  ASSERT_EQ(Network::kMaxRSSI - Network::kMinRSSI + 1,
            arraysize(kExpectedStrengths));

  for (int rssi = Network::kMinRSSI; rssi <= Network::kMaxRSSI; ++rssi) {
    Network network(1, "", kNetworkHome, 0, rssi);
    EXPECT_EQ(kExpectedStrengths[rssi - Network::kMinRSSI],
              network.GetSignalStrength());
  }

  {
    Network network(1, "", kNetworkHome, 0, Network::kMinRSSI - 1);
    EXPECT_EQ(kExpectedStrengths[0], network.GetSignalStrength());
  }

  {
    Network network(1, "", kNetworkHome, 0, Network::kMaxRSSI + 1);
    EXPECT_EQ(kExpectedStrengths[arraysize(kExpectedStrengths) - 1],
              network.GetSignalStrength());
  }
}


}  // namespace shill
