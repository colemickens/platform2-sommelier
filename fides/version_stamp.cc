// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

#include "fides/version_stamp.h"

namespace fides {

namespace {

static const int kInvalidComponent = 0;

}  // namespace

VersionStamp::VersionStamp() {
}

void VersionStamp::Set(const std::string& name, int value) {
  CHECK_LT(0, value);
  clocks_[name] = value;
}

const int& VersionStamp::Get(const std::string& name) const {
  const VersionMap::const_iterator entry = clocks_.find(name);
  if (entry == clocks_.end())
    return kInvalidComponent;
  return entry->second;
}

bool VersionStamp::IsAfter(const VersionStamp& rhs) const {
  return isBefore(rhs.clocks_, clocks_);
}

bool VersionStamp::IsBefore(const VersionStamp& rhs) const {
  return isBefore(clocks_, rhs.clocks_);
}

bool VersionStamp::IsConcurrent(const VersionStamp& rhs) const {
  return !isBefore(clocks_, rhs.clocks_) &&
         !isBefore(rhs.clocks_, clocks_);
}

bool VersionStamp::isBefore(const VersionStamp::VersionMap& lhs,
                            const VersionStamp::VersionMap& rhs) {
  bool strictly_less = false;
  auto il = lhs.begin(), el = lhs.end(), ir = rhs.begin(), er = rhs.end();
  while (il != el && ir != er) {
    // If the iterator of the lhs is ever behind the rhs iterator, lhs cannot
    // be before rhs since lhs has an entry which rhs does not.
    if (il->first < ir->first)
      return false;

    if (il->first == ir->first) {
      // If the logical clock value of a component in lhs is later than rhs, lhs
      // cannot be earlier than rhs.
      if (il->second > ir->second)
        return false;

      // At least one component in lhs must have a value that is strictly less
      // than the corresponding one in rhs or we must find at least one
      // component in rhs which is not in lhs (see below).
      if (il->second < ir->second)
        strictly_less = true;

      ++il;
      ++ir;
    } else if (il->first > ir->first) {
      // We treat absent entries with an implicit default value of 0. Thus if we
      // find at least one component in rhs which is not in lhs then the lhs
      // could be strictly less than rhs. This would mean that the rhs iterator
      // would have to be behind the lhs iterator.
      strictly_less = true;
      ++ir;
    }
  }

  // If the lhs has unconsumed entries lhs cannot be before rhs.
  if (il != el)
    return false;

  // If rhs has unconsumed entries, lhs is strictly less than rhs.
  if (ir != er)
    strictly_less = true;

  return strictly_less;
}

}  // namespace fides
