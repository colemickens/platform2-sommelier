// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_IDENTIFIER_UTILS_H_
#define SETTINGSD_IDENTIFIER_UTILS_H_

#include <map>
#include <set>
#include <string>

namespace settingsd {

namespace utils {

// Returns true if |prefix| is a key, i.e. does not have '.' as the last
// character.
bool IsKey(const std::string& prefix);

// Returns the prefix of the parent namespace for |prefix|. If |prefix| is the
// empty string, returns the empty string.
std::string GetParentPrefix(const std::string& prefix);

// A range adaptor for a pair of iterators.
template <class Iter>
class Range {
 public:
  Range(Iter begin, Iter end) : begin_(begin), end_(end) {}
  Iter begin() { return begin_; }
  Iter end() { return end_; }

 private:
  Iter begin_;
  Iter end_;
};

// Convenience function for creating a Range.
template <class Iter>
Range<Iter> make_range(Iter begin, Iter end) {
  return Range<Iter>(begin, end);
}

// Returns a range covering the prefixes in |container| which start with
// |prefix|. This set does not include |prefix| itself.
template <class T>
Range<typename T::const_iterator> GetChildPrefixes(const std::string& prefix,
                                                   const T& container) {
  // Keys don't have child prefixes.
  if (utils::IsKey(prefix))
    return make_range(container.end(), container.end());

  // For the root prefix, always return the whole range.
  if (prefix.size() == 0)
    return make_range(container.begin(), container.end());

  std::string end_key(prefix);
  end_key[end_key.size() - 1]++;
  return make_range(container.upper_bound(prefix),
                    container.lower_bound(end_key));
}

}  // namespace utils

}  // namespace settingsd

#endif  // SETTINGSD_IDENTIFIER_UTILS_H_
