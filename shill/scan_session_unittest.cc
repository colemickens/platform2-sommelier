// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/scan_session.h"

#include <limits>
#include <set>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_netlink_manager.h"
#include "shill/netlink_manager.h"

using std::set;
using std::vector;
using testing::_;
using testing::ContainerEq;
using testing::Test;

namespace shill {

static const uint16_t kExpectedFreq5640 = 5640;
static const uint16_t kExpectedFreq5600 = 5600;
static const uint16_t kExpectedFreq5580 = 5580;
static const uint16_t kExpectedFreq5560 = 5560;
static const uint16_t kExpectedFreq5620 = 5620;

static WiFiProvider::FrequencyCount kConnectedFrequencies[] = {
  WiFiProvider::FrequencyCount(kExpectedFreq5640, 40),  // 40th percentile.
  WiFiProvider::FrequencyCount(kExpectedFreq5600, 25),  // 65th percentile.
  WiFiProvider::FrequencyCount(kExpectedFreq5580, 20),  // 85th percentile.
  WiFiProvider::FrequencyCount(kExpectedFreq5560, 10),  // 95th percentile.
  WiFiProvider::FrequencyCount(kExpectedFreq5620, 5)    // 100th percentile.
};

static const uint16_t kExpectedFreq2432 = 2432;
static const uint16_t kExpectedFreq2427 = 2427;
static const uint16_t kExpectedFreq2422 = 2422;
static const uint16_t kExpectedFreq2417 = 2417;
static const uint16_t kExpectedFreq2412 = 2412;

static uint16_t kUnconnectedFrequencies[] = {
  kExpectedFreq2432,
  kExpectedFreq2427,
  kExpectedFreq2422,
  kExpectedFreq2417,
  kExpectedFreq2412
};

class ScanSessionTest : public Test {
 public:
  // Test set of "all the other frequencies this device can support" in
  // sorted order.
  ScanSessionTest() {
    WiFiProvider::FrequencyCountList connected_frequencies(
        kConnectedFrequencies,
        kConnectedFrequencies + arraysize(kConnectedFrequencies));

    set<uint16_t> unconnected_frequencies(
        kUnconnectedFrequencies,
        kUnconnectedFrequencies + arraysize(kUnconnectedFrequencies));
    const int kArbitraryMinimum = 1;
    const int kArbitraryMaximum = std::numeric_limits<int>::max();
    ScanSession::OnScanFailed null_error_handler;
    scan_session_.reset(new ScanSession(&netlink_manager_,
                                        NULL,
                                        connected_frequencies,
                                        unconnected_frequencies,
                                        0,
                                        ScanSession::FractionList(),
                                        kArbitraryMinimum,
                                        kArbitraryMaximum,
                                        null_error_handler));
  }

  virtual std::vector<uint16_t> GetScanFrequencies(float scan_fraction,
                                                   size_t min_frequencies,
                                                   size_t max_frequencies) {
    return scan_session_->GetScanFrequencies(scan_fraction, min_frequencies,
                                             max_frequencies);
  }
  ScanSession *scan_session() { return scan_session_.get(); }

 private:
  scoped_ptr<ScanSession> scan_session_;
  MockNetlinkManager netlink_manager_;
};

// Test that we can get a bunch of frequencies up to a specified fraction.
TEST_F(ScanSessionTest, FractionTest) {
  vector<uint16_t> result;

  // Get the first 83% of the connected values.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5640);
    expected.push_back(kExpectedFreq5600);
    expected.push_back(kExpectedFreq5580);
    result = GetScanFrequencies(.83, 1, std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // Get the next 4 values.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5560);
    expected.push_back(kExpectedFreq5620);
    expected.push_back(kExpectedFreq2412);
    expected.push_back(kExpectedFreq2417);
    result = GetScanFrequencies(ScanSession::kAllFrequencies, 1, 4);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // And, get the remaining list.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2422);
    expected.push_back(kExpectedFreq2427);
    expected.push_back(kExpectedFreq2432);
    result = GetScanFrequencies(ScanSession::kAllFrequencies, 20,
                                std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_FALSE(scan_session()->HasMoreFrequencies());
  }
}

// Test that we can get a bunch of frequencies up to a specified fraction,
// followed by another group up to a specified fraction.
TEST_F(ScanSessionTest, TwoFractionsTest) {
  vector<uint16_t> result;

  // Get the first 60% of the connected values.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5640);
    expected.push_back(kExpectedFreq5600);
    result = GetScanFrequencies(.60, 0, std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // Get the next 32% of the connected values.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5580);
    expected.push_back(kExpectedFreq5560);
    result = GetScanFrequencies(.32, 0, std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // And, get the remaining list.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5620);
    expected.push_back(kExpectedFreq2412);
    expected.push_back(kExpectedFreq2417);
    expected.push_back(kExpectedFreq2422);
    expected.push_back(kExpectedFreq2427);
    expected.push_back(kExpectedFreq2432);
    result = GetScanFrequencies(ScanSession::kAllFrequencies,
                                std::numeric_limits<size_t>::max(),
                                std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_FALSE(scan_session()->HasMoreFrequencies());
  }
}

// Test that we can get a bunch of frequencies up to a minimum count, even
// when the requested fraction has already been reached.
TEST_F(ScanSessionTest, MinTest) {
  vector<uint16_t> result;

  // Get the first 3 previously seen values.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5640);
    expected.push_back(kExpectedFreq5600);
    expected.push_back(kExpectedFreq5580);
    result = GetScanFrequencies(.30, 3, std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // Get the next value by requensting a minimum of 1.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5560);
    result = GetScanFrequencies(0.0, 1, std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // And, get the remaining list.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5620);
    expected.push_back(kExpectedFreq2412);
    expected.push_back(kExpectedFreq2417);
    expected.push_back(kExpectedFreq2422);
    expected.push_back(kExpectedFreq2427);
    expected.push_back(kExpectedFreq2432);
    result = GetScanFrequencies(ScanSession::kAllFrequencies,
                                std::numeric_limits<size_t>::max(),
                                std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_FALSE(scan_session()->HasMoreFrequencies());
  }
}

// Test that we can get up to a specified maximum number of frequencies.
TEST_F(ScanSessionTest, MaxTest) {
  vector<uint16_t> result;

  // Get the first 7 values (crosses seen/unseen boundary).
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5640);
    expected.push_back(kExpectedFreq5600);
    expected.push_back(kExpectedFreq5580);
    expected.push_back(kExpectedFreq5560);
    expected.push_back(kExpectedFreq5620);
    expected.push_back(kExpectedFreq2412);
    expected.push_back(kExpectedFreq2417);
    result = GetScanFrequencies(ScanSession::kAllFrequencies, 1, 7);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // And, get the remaining list.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2422);
    expected.push_back(kExpectedFreq2427);
    expected.push_back(kExpectedFreq2432);
    result = GetScanFrequencies(ScanSession::kAllFrequencies, 20,
                                std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_FALSE(scan_session()->HasMoreFrequencies());
  }
}

// Test that we can get exactly the seen frequencies and exactly the unseen
// ones.
TEST_F(ScanSessionTest, ExactTest) {
  vector<uint16_t> result;

  // Get the first 5 values -- exectly on the seen/unseen border.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5640);
    expected.push_back(kExpectedFreq5600);
    expected.push_back(kExpectedFreq5580);
    expected.push_back(kExpectedFreq5560);
    expected.push_back(kExpectedFreq5620);
    result = GetScanFrequencies(ScanSession::kAllFrequencies, 5, 5);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // And, get the last 5.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2412);
    expected.push_back(kExpectedFreq2417);
    expected.push_back(kExpectedFreq2422);
    expected.push_back(kExpectedFreq2427);
    expected.push_back(kExpectedFreq2432);
    result = GetScanFrequencies(ScanSession::kAllFrequencies, 5, 5);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_FALSE(scan_session()->HasMoreFrequencies());
  }
}

// Test that we can get everything in one read.
TEST_F(ScanSessionTest, AllOneReadTest) {

  vector<uint16_t> expected;
  expected.push_back(kExpectedFreq5640);
  expected.push_back(kExpectedFreq5600);
  expected.push_back(kExpectedFreq5580);
  expected.push_back(kExpectedFreq5560);
  expected.push_back(kExpectedFreq5620);
  expected.push_back(kExpectedFreq2412);
  expected.push_back(kExpectedFreq2417);
  expected.push_back(kExpectedFreq2422);
  expected.push_back(kExpectedFreq2427);
  expected.push_back(kExpectedFreq2432);
  vector<uint16_t> result;
  result = GetScanFrequencies(ScanSession::kAllFrequencies,
                              std::numeric_limits<size_t>::max(),
                              std::numeric_limits<size_t>::max());
  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_FALSE(scan_session()->HasMoreFrequencies());
}

// Test that we can get all the previously seen frequencies (and only the
// previously seen frequencies) via the requested fraction.
TEST_F(ScanSessionTest, EverythingFractionTest) {
  vector<uint16_t> result;

  // Get the first 100% of the connected values.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5640);
    expected.push_back(kExpectedFreq5600);
    expected.push_back(kExpectedFreq5580);
    expected.push_back(kExpectedFreq5560);
    expected.push_back(kExpectedFreq5620);
    result = GetScanFrequencies(1.0, 0, std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }

  // And, get the remaining list.
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2412);
    expected.push_back(kExpectedFreq2417);
    expected.push_back(kExpectedFreq2422);
    expected.push_back(kExpectedFreq2427);
    expected.push_back(kExpectedFreq2432);
    result = GetScanFrequencies(ScanSession::kAllFrequencies,
                                std::numeric_limits<size_t>::max(),
                                std::numeric_limits<size_t>::max());
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_FALSE(scan_session()->HasMoreFrequencies());
  }
}

// Test that we can get each value individually.
TEST_F(ScanSessionTest, IndividualReadsTest) {
  vector<uint16_t> result;
  static const float kArbitraryFraction = 0.83;

  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5640);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5600);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5580);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5560);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq5620);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2412);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2417);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2422);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2427);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_TRUE(scan_session()->HasMoreFrequencies());
  }
  {
    vector<uint16_t> expected;
    expected.push_back(kExpectedFreq2432);
    result = GetScanFrequencies(kArbitraryFraction, 1, 1);
    EXPECT_THAT(result, ContainerEq(expected));
    EXPECT_FALSE(scan_session()->HasMoreFrequencies());
  }
}

}  // namespace shill
