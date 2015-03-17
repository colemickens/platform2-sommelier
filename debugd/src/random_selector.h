// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_RANDOM_SELECTOR_H_
#define DEBUGD_SRC_RANDOM_SELECTOR_H_

#include <string>
#include <vector>

namespace debugd {

// RandomSelector can be used to pick vectors of strings according to certain
// probabilities. The probabilities are set using SetOdds(). A randomly picked
// vector can be obtained by calling GetNext().
//
// Sample usage:
//
// RandomSelector random_selector;
// std::vector<RandomSelector::OddsAndValue> odds {
//   {50, {"a"}},
//   {40, {"b"}},
//   {10, {"c"}}
// };
// random_selector.SetOdds(odds);
//
// // The following should return "a" with a probability of 50%,
// // "b" with a probability of 40%, and "c" with a probability of 10%:
//
// std::vector<std::string>& selection = random_selector.GetNext();
class RandomSelector {
 public:
  struct OddsAndValue {
    double weight;
    std::vector<std::string> value;
  };

  virtual ~RandomSelector() {}

  // Read probabilities from a file. The file is a bunch of lines each with:
  // <odds> <corresponding string>
  virtual void SetOddsFromFile(const std::string& filename);

  // Set the probabilities for various strings.
  void SetOdds(const std::vector<OddsAndValue>& odds);

  // Get the next randomly picked string in |next|.
  const std::vector<std::string>& GetNext();

  // Returns the number of string entries.
  size_t GetNumStrings() const {
    return odds_.size();
  }

  // Sum of the |weight| fields in the vector.
  static double SumOdds(const std::vector<OddsAndValue>& odds);

 private:
  // Get a floating point number between 0.0 and |max|.
  virtual double RandDoubleUpTo(double max);

  // Get a string corresponding to a random double |value| that is in our odds
  // vector. Stores the result in |key|.
  const std::vector<std::string>& GetKeyOf(double value);

  // A dictionary representing the strings to choose from and associated odds.
  std::vector<OddsAndValue> odds_;

  double sum_of_odds_;
};

}  // namespace debugd


#endif  // DEBUGD_SRC_RANDOM_SELECTOR_H_
