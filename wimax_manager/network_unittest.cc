// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/network.h"

#include <gtest/gtest.h>

namespace wimax_manager {

class NetworkTest : public testing::Test {
 protected:
  NetworkRefPtr network_;
};

TEST_F(NetworkTest, DecodeCINR) {
  int encoded_cinr = 0;
  int max_encoded_cinr = Network::kMaxCINR - Network::kMinCINR;
  int cinr = Network::kMinCINR;
  while (encoded_cinr <= max_encoded_cinr) {
    EXPECT_EQ(cinr++, Network::DecodeCINR(encoded_cinr++));
  }
  EXPECT_EQ(Network::kMinCINR, Network::DecodeCINR(-1));
  EXPECT_EQ(Network::kMaxCINR, Network::DecodeCINR(max_encoded_cinr + 1));
}

TEST_F(NetworkTest, DecodeRSSI) {
  int encoded_rssi = 0;
  int max_encoded_rssi = Network::kMaxRSSI - Network::kMinRSSI;
  int rssi = Network::kMinRSSI;
  while (encoded_rssi <= max_encoded_rssi) {
    EXPECT_EQ(rssi++, Network::DecodeRSSI(encoded_rssi++));
  }
  EXPECT_EQ(Network::kMinRSSI, Network::DecodeRSSI(-1));
  EXPECT_EQ(Network::kMaxRSSI, Network::DecodeRSSI(max_encoded_rssi + 1));
}

TEST_F(NetworkTest, GetSignalStrength) {
  // clang-format off
  static const int kSignalStrengthTable[5][6] = {
      {0,  0,  0,  0,  0,   0},
      {0,  0,  0, 20, 20,  40},
      {0,  0, 20, 20, 40,  60},
      {0, 20, 20, 40, 60,  80},
      {0, 20, 40, 60, 80, 100},
  };
  // clang-format on

  for (int rssi = Network::kMinRSSI; rssi <= Network::kMaxRSSI; ++rssi) {
    for (int cinr = Network::kMinCINR; cinr <= Network::kMaxCINR; ++cinr) {
      int row = 4;
      if (rssi <= -80) {
        row = 0;
      } else if (rssi <= -75) {
        row = 1;
      } else if (rssi <= -65) {
        row = 2;
      } else if (rssi <= -55) {
        row = 3;
      }

      int column = 5;
      if (cinr <= -3) {
        column = 0;
      } else if (cinr <= 0) {
        column = 1;
      } else if (cinr <= 3) {
        column = 2;
      } else if (cinr <= 10) {
        column = 3;
      } else if (cinr <= 15) {
        column = 4;
      }
      network_ = new Network(1, "", kNetworkHome, cinr, rssi);
      EXPECT_EQ(kSignalStrengthTable[row][column],
                network_->GetSignalStrength());
    }
  }
}

TEST_F(NetworkTest, GetNameWithIdentifier) {
  network_ = new Network(0xabcd, "", kNetworkHome, 0, 0);
  EXPECT_EQ("network (0x0000abcd)", network_->GetNameWithIdentifier());

  network_ = new Network(0xabcd, "My Net", kNetworkHome, 0, 0);
  EXPECT_EQ("network 'My Net' (0x0000abcd)", network_->GetNameWithIdentifier());
}

}  // namespace wimax_manager
