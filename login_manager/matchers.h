// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MATCHERS_H_
#define LOGIN_MANAGER_MATCHERS_H_

#include <algorithm>

#include <gmock/gmock.h>

namespace login_manager {

// Forces arg to an array of char and compares to str for equality.
MATCHER_P(CastEq, str, "") {
  return std::equal(str.begin(), str.end(), reinterpret_cast<const char*>(arg));
}

MATCHER_P(VectorEq, str, "") {
  return str.size() == arg.size() &&
      std::equal(str.begin(), str.end(), arg.begin());
}

// Serializes the protobuf in arg to a string, compares to str for equality.
MATCHER_P(PolicyStrEq, str, "") {
  return arg.SerializeAsString() == str;
}

MATCHER_P(StatusEq, status, "") {
  return (arg.owner_key_file_state == status.owner_key_file_state &&
          arg.policy_file_state == status.policy_file_state &&
          arg.defunct_prefs_file_state == status.defunct_prefs_file_state);
}

MATCHER_P(PolicyEq, policy, "") {
  return arg.SerializeAsString() == policy.SerializeAsString();
}

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MATCHERS_H_
