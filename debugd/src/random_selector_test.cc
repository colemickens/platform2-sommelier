// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <gtest/gtest.h>
#include <map>
#include <string>

#include "debugd/src/random_selector.h"

using debugd::RandomSelector;

namespace {

// A large number to do a lot of tests so the probability distribution of the
// results is close to the expected distribution.
const int kLargeNumber = 2000;

// A small floating point number used to verify that the expected odds are equal
// to the odds set.
const float kEpsilon = 0.01f;

// A test file that contains some odds.
const char kOddsFilename[] = "../src/testdata/simple_odds_file.txt";

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
    float abs_diff = fabsf(results_ratio - odds_ratio);
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
  float GetFloatBetween(float min, float max) override {
    current_index_ = (current_index_ + 1) % kLargeNumber;
    return (max - min) * current_index_ / kLargeNumber + min;
  }

  // Stores the current position we are at in the interval between |min| and
  // |max|. See the function GetFloatBetween for details on how this is used.
  int current_index_;
};

// Use the random_selector to generate some values. The number of values to
// generate is |odds_size| * |kLargeNumber|.
void GenerateResults(size_t odds_size,
                     RandomSelector* random_selector,
                     std::map<std::string, float>* results) {
  for (size_t i = 0; i < kLargeNumber * odds_size; ++i) {
    std::string next_value;
    random_selector->GetNext(&next_value);
    (*results)[next_value]++;
  }
}

// Test the RandomSelector class given the odds in the |odds| map.
void TestOdds(const std::map<std::string, float> odds) {
  RandomSelectorWithCustomRNG random_selector;
  random_selector.SetOdds(odds);
  // Generate a lot of values.
  std::map<std::string, float> results;
  GenerateResults(odds.size(), &random_selector, &results);
  // Ensure the values and odds are related.
  CheckResultsAgainstOdds(odds, results);
}

}  // namespace

// Ensure RandomSelector is able to generate results from given odds.
TEST(RandomSelector, GenerateTest) {
  std::map<std::string, float> odds;
  odds["a"] = 1;
  odds["b"] = 2;
  odds["c"] = 3;
  TestOdds(odds);
}

// Ensure RandomSelector is able to read odds from a file.
#if 0  // Appears to be flaky: http://crbug.com/399579
TEST(RandomSelector, SetOddsFromFileTest) {
  RandomSelector random_selector;
  random_selector.SetOddsFromFile(std::string(kOddsFilename));
  std::map<std::string, float> results;
  std::map<std::string, float> odds;
  odds["afile"] = 3;
  odds["bfile"] = 2;
  odds["cfile"] = 1;
  GenerateResults(odds.size(), &random_selector, &results);
  CheckResultsAgainstOdds(odds, results);
}
#endif

// Ensure RandomSelector is able to delete odds that it has previously stored.
TEST(RandomSelector, RemoveEntriesTest) {
  RandomSelector random_selector;
  random_selector.SetOddsFromFile(std::string(kOddsFilename));
  std::string key;

  // Get a key and verify.
  random_selector.GetNext(&key);
  EXPECT_FALSE(key.empty());
  size_t old_size = random_selector.GetNumStrings();
  EXPECT_GT(old_size, 0);

  // Remove the key.
  random_selector.Remove(key);
  EXPECT_EQ(old_size - 1, random_selector.GetNumStrings());
}
