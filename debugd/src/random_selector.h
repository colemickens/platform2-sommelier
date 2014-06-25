// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANDOM_SELECTOR_H_
#define RANDOM_SELECTOR_H_

#include <map>
#include <string>

namespace debugd {

// Returns the sum of the values of map |to_sum|.
float GetSumOfMapValues(const std::map<std::string, float> to_sum);

// RandomSelector is a class that can be used to pick strings according to
// certain probabilities. The probabilities are set using SetOdds(). A randomly
// picked string can be got by calling GetNext().
//
// Sample usage:
//
// RandomSelector random_selector;
// std::map<std::string, float> odds;
// odds["a"] = 50;
// odds["b"] = 40;
// odds["c"] = 10;
// random_selector.SetOdds(odds);
//
// The following should give you "a" with a probability of 50%, "b" with a
// probability of 40% and "c" with a probability of 10%.
//
// std::string selection;
// random_selector.GetNext(&selection);
class RandomSelector {
 public:
  // Read probabilities from a file. The file is a bunch of lines each with:
  // <odds> <corresponding string>
  void SetOddsFromFile(const std::string& filename);

  // Set the probabilities for various strings.
  void SetOdds(const std::map<std::string, float>& odds);

  // Get the next randomly picked string in |next|.
  void GetNext(std::string* next);

  // Removes an entry from the current odds container. If the entry is not found
  // in |odds_|, this function has no effect.
  void Remove(const std::string& key);

  // Returns the number of string entries.
  size_t GetNumStrings() const {
    return odds_.size();
  }

 private:
  // Get a floating point number between |min| and |max|.
  virtual float GetFloatBetween(float min, float max);

  // Get a string corresponding to a random float |value| that is in our odds
  // dictionary. Stores the result in |key|.
  void GetKeyOf(float value, std::string* key);

  // A dictionary representing the strings to choose from and associated odds.
  std::map<std::string, float> odds_;
};

}  // namespace debugd


#endif  // RANDOM_SELECTOR_H_
