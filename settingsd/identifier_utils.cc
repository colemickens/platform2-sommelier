// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/identifier_utils.h"

namespace settingsd {

namespace utils {

bool IsKey(const std::string& prefix) {
  if (prefix.size() == 0u)
    return false;
  return prefix[prefix.size() - 1] != '.';
}

std::string GetParentPrefix(const std::string& prefix) {
  if (prefix.size() < 2u)
    return std::string();
  size_t position =
      prefix.find_last_of(".", prefix.size() - (IsKey(prefix) ? 1 : 2));
  if (position == std::string::npos)
    return std::string();
  return prefix.substr(0u, position + 1u);
}

}  // namespace utils

}  // namespace settingsd
