// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/key.h"

#include <algorithm>

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

bool Key::operator<=(const Key& rhs) const {
  return key_ <= rhs.key_;
}

bool Key::operator==(const Key& rhs) const {
  return key_ == rhs.key_;
}

const std::string& Key::ToString() const {
  return key_;
}

Key Key::GetParent() const {
  size_t position = key_.rfind('.');
  if (position == std::string::npos)
    return Key();
  return Key(key_.substr(0u, position));
}

Key Key::Append(const Key& other) const {
  if (other.IsRootKey())
    return *this;

  if (IsRootKey())
    return other;

  return Key(key_ + "." + other.key_);
}

Key Key::Extend(std::initializer_list<std::string> components) const {
  return Append(Key(JoinComponents(components)));
}

Key Key::CommonPrefix(const Key& other) const {
  size_t last_component_boundary = 0;
  const size_t min_size = std::min(key_.size(), other.key_.size());
  for (size_t i = 0; i < min_size; ++i) {
    if (key_[i] != other.key_[i])
      return Key(key_.substr(0, last_component_boundary));
    if (key_[i] == '.')
      last_component_boundary = i;
  }

  // If both keys have a component boundary at |min_size|, the common prefix
  // ends at |min_size|.
  if ((key_.size() == min_size || key_[min_size] == '.') &&
      (other.key_.size() == min_size || other.key_[min_size] == '.')) {
    return Key(key_.substr(0, min_size));
  }

  return Key(key_.substr(0, last_component_boundary));
}

bool Key::Suffix(const Key& prefix, Key* suffix) const {
  if (prefix.IsRootKey()) {
    *suffix = *this;
    return true;
  }

  if (!prefix.IsPrefixOf(*this))
    return false;

  *suffix = Key(key_.substr(std::min(key_.size(), prefix.key_.size() + 1)));
  return true;
}

Key Key::PrefixUpperBound() const {
  return Key(key_ + '0');
}

bool Key::IsRootKey() const {
  return key_.empty();
}

bool Key::IsPrefixOf(const Key& other) const {
  return *this == CommonPrefix(other);
}

}  // namespace settingsd
