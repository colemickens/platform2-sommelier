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
const double kEpsilon = 0.01;

// A test file that contains some odds.
const char kOddsFilename[] = "../src/testdata/simple_odds_file.txt";

// This function tests whether the results are close enough to the odds (within
// 1%).
void CheckResultsAgainstOdds(
    const std::vector<RandomSelector::OddsAndValue>& odds,
    const std::map<std::string, int>& results) {
  EXPECT_EQ(odds.size(), results.size());

  const double odds_sum = RandomSelector::SumOdds(odds);
  int results_sum = 0;
  for (const auto& item : results) {
    results_sum += item.second;
  }

  for (const auto& odd : odds) {
    const auto result = results.find(odd.value);
    EXPECT_NE(result, results.end());
    const double results_ratio = 1.0*result->second / results_sum;
    const double odds_ratio = odd.weight / odds_sum;
    const double abs_diff = fabs(results_ratio - odds_ratio);
    EXPECT_LT(abs_diff, kEpsilon);
  }
}

// A class that overrides RandDoubleUpTo() to not be random. The number
// generator will emulate a uniform distribution of numbers between 0.0 and
// |max| when called with the same |max| parameter and a whole multiple of
// |random_period| times. This allows better testing of the RandomSelector
// class.
class RandomSelectorWithCustomRNG : public RandomSelector {
 public:
  explicit RandomSelectorWithCustomRNG(unsigned int random_period)
      : random_period_(random_period), current_index_(0) {}

 private:
  // This function returns floats between 0.0 and |max| in an increasing
  // fashion at regular intervals.
  double RandDoubleUpTo(double max) override {
    current_index_ = (current_index_ + 1) % random_period_;
    return max * current_index_ / random_period_;
  }

  // Period (number of calls) over which the fake RNG repeats.
  const unsigned int random_period_;

  // Stores the current position we are at in the interval between 0.0 and
  // |max|. See the function RandDoubleUpTo for details on how this is used.
  int current_index_;
};

// Use the random_selector to generate some values. The number of values to
// generate is |iterations|.
void GenerateResults(size_t iterations,
                     RandomSelector* random_selector,
                     std::map<std::string, int>* results) {
  for (size_t i = 0; i < iterations; ++i) {
    std::string next_value = random_selector->GetNext();
    (*results)[next_value]++;
  }
}

// Test the RandomSelector class given the odds in the |odds| list.
void TestOdds(const std::vector<RandomSelector::OddsAndValue>& odds) {
  RandomSelectorWithCustomRNG random_selector(kLargeNumber);
  random_selector.SetOdds(odds);
  // Generate a lot of values.
  std::map<std::string, int> results;
  GenerateResults(kLargeNumber, &random_selector, &results);
  // Ensure the values and odds are related.
  CheckResultsAgainstOdds(odds, results);
}

}  // namespace

// Ensure RandomSelector is able to generate results from given odds.
TEST(RandomSelector, GenerateTest) {
  std::vector<RandomSelector::OddsAndValue> odds = {
    {1, "a"},
    {2, "b"},
    {3, "c"},
  };
  TestOdds(odds);
}

// Ensure RandomSelector is able to read odds from a file.
TEST(RandomSelector, SetOddsFromFileTest) {
  RandomSelectorWithCustomRNG random_selector(kLargeNumber);
  random_selector.SetOddsFromFile(std::string(kOddsFilename));
  std::map<std::string, int> results;
  std::vector<RandomSelector::OddsAndValue> odds = {
    {3, "afile"},
    {2, "bfile"},
    {1, "cfile"},
  };
  GenerateResults(kLargeNumber, &random_selector, &results);
  CheckResultsAgainstOdds(odds, results);
}
