// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <cstdlib>
#include <string>

#include <base/logging.h>

#include "random_selector.h"

namespace debugd {

float GetSumOfMapValues(const std::map<std::string, float> map_to_sum) {
  float sum = 0.0f;
  std::map<std::string, float>::const_iterator it;
  for (it = map_to_sum.begin(); it != map_to_sum.end(); ++it)
    sum += it->second;
  return sum;
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

float RandomSelector::GetFloatBetween(float min, float max) {
  CHECK_GT(max, min);
  float random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
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
