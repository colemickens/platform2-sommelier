// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_KEY_H_
#define SETTINGSD_KEY_H_

#include <string>

namespace settingsd {

class Key {
 public:
  // Default constructor which initializes Key as the root key.
  Key() = default;

  // Construct a Key from the string representation |key|.
  explicit Key(const std::string& key);

  // These operators forward the respective operations to their std::string
  // equivalents for |key_|.
  bool operator<(const Key& rhs) const;
  bool operator==(const Key& rhs) const;

  // Returns a string representation of the key.
  const std::string& ToString() const;

  // Returns the parent key for |key|. If |key| the root key, returns the root
  // key.
  Key GetParent() const;

  // Returns true, if this key is the root key, i.e. has the empty string as its
  // string representation.
  bool IsRootKey() const;

 private:
  // Checks whether |key| is the string representation of a valid key.
  bool IsValidKey(const std::string& key) const;

  std::string key_;
};

}  // namespace settingsd

#endif  // SETTINGSD_KEY_H_
