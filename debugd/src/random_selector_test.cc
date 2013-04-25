// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <map>
#include <string>

#include "random_selector.h"

using debugd::RandomSelector;

namespace {

// A large number to do a lot of tests so the probability distribution of the
// results is close to the expected distribution.
const int kLargeNumber = 1000;

// A small floating point number used to verify that the expected odds are equal
// to the odds set.
const float kEpsilon = 0.01f;

// This function tests whether the results are close enough to the odds (within
// 1%).
void CheckResultsAgainstOdds(const std::map<std::string, float> odds,
                             const std::map<std::string, float> results) {
  EXPECT_EQ(odds.size(), results.size());

  float odds_sum = debugd::GetSumOfMapValues(odds);
  float results_sum = debugd::GetSumOfMapValues(results);

  for (std::map<std::string, float>::const_iterator odd = odds.begin();
       odd != odds.end();
       ++odd) {
    std::map<std::string, float>::const_iterator result =
        results.find(odd->first);
    EXPECT_NE(result, results.end());
    float results_ratio = result->second / results_sum;
    float odds_ratio = odd->second / odds_sum;
    float abs_diff = abs(results_ratio - odds_ratio);
    EXPECT_LT(abs_diff, kEpsilon);
  }

}

// A class that overrides GetFloatBetween to not use the rand() function. This
// allows better testing of the RandomSelector class.
class RandomSelectorWithCustomRNG : public RandomSelector {
 public:
  RandomSelectorWithCustomRNG() : current_index_(0) {}

 private:
  // This function returns floats between |min| and |max| in an increasing
  // fashion at regular intervals.
  virtual float GetFloatBetween(float min, float max) {
    current_index_ = (current_index_ + 1) % kLargeNumber;
    return (max - min) * current_index_ / kLargeNumber + min;
  }

  // Stores the current position we are at in the interval between |min| and
  // |max|. See the function GetFloatBetween for details on how this is used.
  int current_index_;
};

// Test the RandomSelector class given the odds in the |odds| map.
void TestOdds(const std::map<std::string, float> odds) {
  RandomSelectorWithCustomRNG random_selector;
  random_selector.SetOdds(odds);
  // Generate a lot of values.
  std::map<std::string, float> results;
  for (size_t i = 0; i < kLargeNumber * odds.size(); ++i) {
    std::string next_value;
    random_selector.GetNext(&next_value);
    results[next_value]++;
  }
  // Ensure the values and odds are related.
  CheckResultsAgainstOdds(odds, results);
}

}  // namespace

TEST(RandomSelector, GenerateTest) {
  std::map<std::string, float> odds;
  odds["a"] = 1;
  odds["b"] = 2;
  odds["c"] = 3;
  TestOdds(odds);
}

