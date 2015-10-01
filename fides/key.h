// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_KEY_H_
#define FIDES_KEY_H_

#include <initializer_list>
#include <string>

namespace fides {

class Key {
 public:
  // Check whether the provided string is a valid key.
  static bool IsValidKey(const std::string& key_string);

  // Default constructor which initializes Key as the root key.
  Key() = default;

  // Construct a Key from the string representation |key|.
  explicit Key(const std::string& key);

  // Construct a Key from the provided components.
  explicit Key(std::initializer_list<std::string> components);

  // These operators forward the respective operations to their std::string
  // equivalents for |key_|.
  bool operator<(const Key& rhs) const;
  bool operator<=(const Key& rhs) const;
  bool operator==(const Key& rhs) const;
  bool operator!=(const Key& rhs) const;

  // Returns a string representation of the key.
  const std::string& ToString() const;

  // Returns the parent key for |key|. If |key| the root key, returns the root
  // key.
  Key GetParent() const;

  // Appends another key as a suffix.
  Key Append(const Key& other) const;

  // Extends a key by appending the specified components.
  Key Extend(std::initializer_list<std::string> components) const;

  // Splits off the first key component and returns its value, as well as the
  // suffix. Returns an empty prefix and suffix for the root key. |suffix| may
  // be nullptr, in which case the suffix doesn't get returned.
  Key Split(Key *suffix) const;

  // Computes the key that is the common prefix of |this| and |other|.
  Key CommonPrefix(const Key& other) const;

  // Returns true and fills in |suffix| if |prefix| is a prefix of this. Returns
  // false and doesn't change |suffix| otherwise.
  bool Suffix(const Key& prefix, Key* suffix) const;

  // Computes the key that forms the upper bound of the subtree rooted at |this|
  // in lexicographic sort order. In other words, the return value is the first
  // key in order that is after every key for which IsPrefixOf() returns true.
  Key PrefixUpperBound() const;

  // Returns true, if this key is the root key, i.e. has the empty string as its
  // string representation.
  bool IsRootKey() const;

  // Checks whether this key is a prefix of |other|. Returns true also if the
  // keys are identical.
  bool IsPrefixOf(const Key& other) const;

 private:
  std::string key_;
};

}  // namespace fides

#endif  // FIDES_KEY_H_
