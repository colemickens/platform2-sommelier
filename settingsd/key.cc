// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/key.h"

#include <base/logging.h>

namespace settingsd {

namespace {

bool IsValidKeyComponentChar(char c) {
  // This corresponds to the set of valid chars in C identifiers.
  //
  // NB: Dashes ('-', i.e. ASCII code 0x2d) are not allowed. That way, all
  // permitted characters sort after '.', which results in prefixes appearing
  // before all matching suffixes in lexicographic sort order. This is helpful
  // when keys and prefixes are used in STL containers such as std::set and
  // std::map.
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

std::string JoinComponents(std::initializer_list<std::string> components) {
  auto component = components.begin();
  if (component == components.end())
    return std::string();

  std::string result = *component;
  for (++component; component != components.end(); ++component) {
    result += ".";
    result += *component;
  }

  return result;
}

}  // namespace

// static
bool Key::IsValidKey(const std::string& key_string) {
  if (key_string.empty())
    return true;

  bool last_char_was_dot = true;
  for (std::string::value_type c : key_string) {
    if (c == '.') {
      // Don't allow successive dots, i.e. empty components.
      if (last_char_was_dot)
        return false;
      last_char_was_dot = true;
    } else {
      if (!IsValidKeyComponentChar(c))
        return false;
      last_char_was_dot = false;
    }
  }

  return !last_char_was_dot;
}

Key::Key(const std::string& key) : key_(key) {
  CHECK(IsValidKey(key_));
}

Key::Key(std::initializer_list<std::string> components)
  : Key(JoinComponents(components)) {}

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

Key Key::Append(const Key& other) const {
  return IsRootKey() ? other : Key(key_ + "." + other.key_);
}

Key Key::Extend(std::initializer_list<std::string> components) const {
  return Append(Key(JoinComponents(components)));
}

bool Key::IsRootKey() const {
  return key_.empty();
}

}  // namespace settingsd
