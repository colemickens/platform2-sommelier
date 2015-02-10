// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/random_selector.h"

#include <cstdlib>
#include <fstream>  // NOLINT
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace {

// The space character.
const char kWhitespace = ' ';

}  // namespace

namespace debugd {

double RandomSelector::SumOdds(const std::vector<OddsAndValue>& odds) {
  double sum = 0.0;
  for (const auto& odd : odds) {
    sum += odd.weight;
  }
  return sum;
}

void RandomSelector::SetOddsFromFile(const std::string& filename) {
  odds_.clear();

  std::ifstream infile(filename.c_str());
  CHECK(infile.good());
  std::string line;
  while (std::getline(infile, line)) {
    std::vector<std::string> tokens;
    base::SplitString(line, kWhitespace, &tokens);
    VLOG(1) << "line is: " << line;
    VLOG(1) << "tokens[0] is: " << tokens[0] << "end";
    VLOG(1) << "tokens[1] is: " << tokens[1] << "end";
    CHECK_GT(tokens.size(), 1U);
    double odd;
    CHECK(base::StringToDouble(tokens[0], &odd));
    tokens.erase(tokens.begin(), tokens.begin() + 1);
    std::string value = JoinString(tokens, kWhitespace);
    odds_.push_back({odd, value});
  }
  sum_of_odds_ = SumOdds(odds_);
}

void RandomSelector::SetOdds(const std::vector<OddsAndValue>& odds) {
  odds_ = odds;
  sum_of_odds_ = SumOdds(odds_);
}

const std::string& RandomSelector::GetNext() {
  // Get a random double between 0 and the sum.
  double random = RandDoubleUpTo(sum_of_odds_);
  // Figure out what it belongs to.
  return GetKeyOf(random);
}

double RandomSelector::RandDoubleUpTo(double max) {
  CHECK_GT(max, 0.0);
  return max * base::RandDouble();
}

const std::string& RandomSelector::GetKeyOf(double value) {
  double current = 0.0;
  for (const auto& odd : odds_) {
    current += odd.weight;
    if (value < current) {
      return odd.value;
    }
  }
  NOTREACHED() << "Invalid value for key: " << value;
  static const std::string kEmptyString;
  return kEmptyString;
}

}  // namespace debugd
