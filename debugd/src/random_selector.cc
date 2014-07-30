// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/random_selector.h"

#include <cstdlib>
#include <fstream>  // NOLINT
#include <map>
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

float GetSumOfMapValues(const std::map<std::string, float> map_to_sum) {
  float sum = 0.0f;
  std::map<std::string, float>::const_iterator it;
  for (it = map_to_sum.begin(); it != map_to_sum.end(); ++it)
    sum += it->second;
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
    odds_[value] = static_cast<float>(odd);
  }
}

void RandomSelector::SetOdds(const std::map<std::string, float>& odds) {
  odds_.clear();
  odds_.insert(odds.begin(), odds.end());
}

void RandomSelector::GetNext(std::string* next) {
  // Sum up all the odds.
  float sum = GetSumOfMapValues(odds_);
  // Get a random float between 0 and the sum.
  float random = GetFloatBetween(0.0f, sum);
  // Figure out what it belongs to.
  GetKeyOf(random, next);
}

void RandomSelector::Remove(const std::string& key) {
  std::map<std::string, float>::iterator iter = odds_.find(key);
  if (iter != odds_.end()) {
    odds_.erase(iter);
  }
}

float RandomSelector::GetFloatBetween(float min, float max) {
  CHECK_GT(max, min);
  float random = static_cast<float>(base::RandDouble());
  return random * (max - min) + min;
}

void RandomSelector::GetKeyOf(float value, std::string* key) {
  float current = 0.0f;
  std::map<std::string, float>::const_iterator it;
  for (it = odds_.begin(); it != odds_.end(); ++it) {
    current += it->second;
    if (value <= current) {
      *key = it->first;
      return;
    }
  }

  NOTREACHED() << "Invalid value for key.";
}

}  // namespace debugd
