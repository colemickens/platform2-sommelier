// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_IDENTIFIER_UTILS_H_
#define FIDES_IDENTIFIER_UTILS_H_

#include <map>
#include <set>
#include <string>

#include "fides/key.h"

namespace fides {

namespace utils {

// A range adaptor for a pair of iterators.
template <class Iter>
class Range {
 public:
  Range(Iter begin, Iter end) : begin_(begin), end_(end) {}
  Iter begin() const { return begin_; }
  Iter end() const { return end_; }


 private:
  const Iter begin_;
  const Iter end_;
};

// Convenience function for creating a Range.
template <class Iter>
Range<Iter> make_range(Iter begin, Iter end) {
  return Range<Iter>(begin, end);
}

// Returns a range of identifiers defined in |container| that are either equal
// to |key| or have |key| as their ancestor. If |key| is the empty string,
// returns the full range of the |container|. If |key| is not a valid key,
// returns the empty range.
template <class T>
Range<typename T::const_iterator> GetRange(const Key& key, const T& container) {
  // For the root key, always return the whole range.
  if (key.IsRootKey())
    return make_range(container.begin(), container.end());

  return make_range(container.lower_bound(key),
                    container.lower_bound(key.PrefixUpperBound()));
}

// Checks whether |container| has any keys which are equal to or have |key| as
// an ancestor.
template <class T>
bool HasKeys(const Key& key, const T& container) {
  auto range = GetRange(key, container);
  return range.begin() != range.end();
}

}  // namespace utils

}  // namespace fides

#endif  // FIDES_IDENTIFIER_UTILS_H_
