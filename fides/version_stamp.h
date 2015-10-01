// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_VERSION_STAMP_H_
#define FIDES_VERSION_STAMP_H_

#include <map>
#include <string>

namespace fides {

// Implementation of a vector clock. See also:
// http://en.wikipedia.org/wiki/Vector_clock for an explanation of vector
// clocks.
class VersionStamp {
 public:
  VersionStamp();

  // Sets the entry for the clock with key |name|. |value| must be >= 1.
  void Set(const std::string& name, int value);

  // If |clocks_| contains a clock entry with key |name|, this method returns
  // the corresponding clock value, otherwise it returns 0.
  const int& Get(const std::string& name) const;

  // Returns true, if there is a causal relationship to the vector clock |rhs|
  // and |rhs| is later than this vector clock.
  bool IsAfter(const VersionStamp& rhs) const;

  // Returns true, if there is a causal relationship to the vector clock |rhs|
  // and |rhs| is earlier than this vector clock.
  bool IsBefore(const VersionStamp& rhs) const;

  // Returns true, if there is no causal relationship between this vector clock
  // and |rhs|. The events have thus to be considered concurrent.
  bool IsConcurrent(const VersionStamp& rhs) const;

 private:
  using VersionMap = std::map<std::string, int>;
  VersionMap clocks_;

  static bool isBefore(const VersionStamp::VersionMap& lhs,
                       const VersionStamp::VersionMap& rhs);
};

}  // namespace fides

#endif  // FIDES_VERSION_STAMP_H_
