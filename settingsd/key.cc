// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/key.h"

#include <base/logging.h>

namespace settingsd {

Key::Key(const std::string& key) : key_(key) {
  CHECK(IsValidKey(key_));
}

bool Key::operator<(const Key& rhs) const {
  return key_ < rhs.key_;
}

bool Key::operator==(const Key& rhs) const {
  return key_ == rhs.key_;
}

const std::string& Key::ToString() const {
  return key_;
}

Key Key::GetParent() const {
  size_t position = key_.find_last_of(".");
  if (position == std::string::npos)
    return Key();
  return Key(key_.substr(0u, position));
}

bool Key::IsRootKey() const {
  return key_.empty();
}


bool Key::IsValidKey(const std::string& key) const {
  // TODO(cschuet): replace this with a more rigorous validity check.
  if (key.size() == 0u)
    return true;
  return key[key.size() - 1] != '.';
}

}  // namespace settingsd
