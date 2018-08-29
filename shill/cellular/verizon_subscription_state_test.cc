//
// Copyright 2018 The Chromium OS Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/cellular/verizon_subscription_state.h"

#include <memory>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

using std::make_tuple;
using std::vector;

namespace shill {

class VerizonSubscriptionStateInvalidPcoTest
    : public testing::TestWithParam<std::vector<uint8_t>> {};

TEST_P(VerizonSubscriptionStateInvalidPcoTest,
       FindVerizonSubscriptionStateFromPco) {
  const auto& raw_data = GetParam();
  std::unique_ptr<CellularPco> pco = CellularPco::CreateFromRawData(raw_data);
  ASSERT_NE(nullptr, pco);
  SubscriptionState subscription_state = SubscriptionState::kUnknown;
  EXPECT_FALSE(FindVerizonSubscriptionStateFromPco(*pco, &subscription_state));
  EXPECT_EQ(SubscriptionState::kUnknown, subscription_state);
}

INSTANTIATE_TEST_CASE_P(
    VerizonSubscriptionStateInvalidPcoTest,
    VerizonSubscriptionStateInvalidPcoTest,
    testing::Values(
        // Verizon-specific PCO not found
        vector<uint8_t>({0x27, 0x04, 0x00, 0xAA, 0xBB, 0x00}),
        // Malformed Verizon-specific PCO
        vector<uint8_t>({0x27, 0x04, 0x00, 0xFF, 0x00, 0x00}),
        vector<uint8_t>({0x27, 0x07, 0x00, 0xFF, 0x00, 0x03, 0x13, 0x01, 0x84}),
        vector<uint8_t>(
            {0x27, 0x08, 0x00, 0xFF, 0x00, 0x04, 0xEE, 0x01, 0x84, 0x00}),
        vector<uint8_t>(
            {0x27, 0x08, 0x00, 0xFF, 0x00, 0x04, 0x13, 0xEE, 0x84, 0x00}),
        vector<uint8_t>(
            {0x27, 0x08, 0x00, 0xFF, 0x00, 0x04, 0x13, 0x01, 0xEE, 0x00}),
        vector<uint8_t>({
            // clang-format off
            0x27, 0x09, 0x00, 0xFF, 0x00, 0x05, 0x13, 0x01, 0xEE, 0x00, 0x00
            // clang-format on
        })));

using VerizonSubscriptionStateTestParams =
    std::tuple<std::vector<uint8_t>,  // raw data
               SubscriptionState>;    // expected subscription state

class VerizonSubscriptionStateTest
    : public testing::TestWithParam<VerizonSubscriptionStateTestParams> {};

TEST_P(VerizonSubscriptionStateTest, FindVerizonSubscriptionStateFromPco) {
  const auto& raw_data = std::get<0>(GetParam());
  SubscriptionState expected_subscription_state = std::get<1>(GetParam());
  std::unique_ptr<CellularPco> pco = CellularPco::CreateFromRawData(raw_data);
  ASSERT_NE(nullptr, pco);
  SubscriptionState subscription_state = SubscriptionState::kUnknown;
  EXPECT_TRUE(FindVerizonSubscriptionStateFromPco(*pco, &subscription_state));
  EXPECT_EQ(expected_subscription_state, subscription_state);
}

INSTANTIATE_TEST_CASE_P(
    VerizonSubscriptionStateTest,
    VerizonSubscriptionStateTest,
    testing::Values(
        make_tuple(
            vector<uint8_t>(
                {0x27, 0x08, 0x00, 0xFF, 0x00, 0x04, 0x13, 0x01, 0x84, 0x00}),
            SubscriptionState::kProvisioned),
        make_tuple(
            vector<uint8_t>(
                {0x27, 0x08, 0x00, 0xFF, 0x00, 0x04, 0x13, 0x01, 0x84, 0x03}),
            SubscriptionState::kOutOfCredits),
        make_tuple(
            vector<uint8_t>(
                {0x27, 0x08, 0x00, 0xFF, 0x00, 0x04, 0x13, 0x01, 0x84, 0x05}),
            SubscriptionState::kUnprovisioned),
        make_tuple(
            vector<uint8_t>(
                {0x27, 0x08, 0x00, 0xFF, 0x00, 0x04, 0x13, 0x01, 0x84, 0xFF}),
            SubscriptionState::kUnknown)));

}  // namespace shill
