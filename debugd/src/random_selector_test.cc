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

// This function tests whether the results are close enough to the odds (within
// 1%).
void CheckResultsAgainstOdds(const std::map<std::string, double> odds,
                             const std::map<std::string, double> results) {
  EXPECT_EQ(odds.size(), results.size());

  double odds_sum = debugd::GetSumOfMapValues(odds);
  double results_sum = debugd::GetSumOfMapValues(results);

  for (std::map<std::string, double>::const_iterator odd = odds.begin();
       odd != odds.end();
       ++odd) {
    std::map<std::string, double>::const_iterator result =
        results.find(odd->first);
    EXPECT_NE(result, results.end());
    double results_ratio = result->second / results_sum;
    double odds_ratio = odd->second / odds_sum;
    double abs_diff = fabs(results_ratio - odds_ratio);
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
                     std::map<std::string, double>* results) {
  for (size_t i = 0; i < iterations; ++i) {
    std::string next_value;
    random_selector->GetNext(&next_value);
    (*results)[next_value]++;
  }
}

// Test the RandomSelector class given the odds in the |odds| map.
void TestOdds(const std::map<std::string, double> odds) {
  RandomSelectorWithCustomRNG random_selector(kLargeNumber);
  random_selector.SetOdds(odds);
  // Generate a lot of values.
  std::map<std::string, double> results;
  GenerateResults(kLargeNumber, &random_selector, &results);
  // Ensure the values and odds are related.
  CheckResultsAgainstOdds(odds, results);
}

}  // namespace

// Ensure RandomSelector is able to generate results from given odds.
TEST(RandomSelector, GenerateTest) {
  std::map<std::string, double> odds;
  odds["a"] = 1;
  odds["b"] = 2;
  odds["c"] = 3;
  TestOdds(odds);
}

// Ensure RandomSelector is able to read odds from a file.
#if 0  // Appears to be flaky: http://crbug.com/399579
// A test file that contains some odds.
const char kOddsFilename[] = "../src/testdata/simple_odds_file.txt";

TEST(RandomSelector, SetOddsFromFileTest) {
  RandomSelector random_selector;
  random_selector.SetOddsFromFile(std::string(kOddsFilename));
  std::map<std::string, double> results;
  std::map<std::string, double> odds;
  odds["afile"] = 3;
  odds["bfile"] = 2;
  odds["cfile"] = 1;
  GenerateResults(kLargeNumber, &random_selector, &results);
  CheckResultsAgainstOdds(odds, results);
}
#endif
