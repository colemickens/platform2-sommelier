// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_REGISTRY_DICT_H_
#define AUTHPOLICY_POLICY_REGISTRY_DICT_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace base {
class Value;
}

namespace policy {

// A case-insensitive string comparison functor.
struct CaseInsensitiveStringCompare {
  bool operator()(const std::string& a, const std::string& b) const;
};

// In-memory representation of a registry subtree. Using a
// base::DictionaryValue directly seems tempting, but that doesn't handle the
// registry's case-insensitive-but-case-preserving semantics properly.
//
// Code was copied from Chromium and slightly modified. Will be refactored to
// simplify maintainance, see crbug.com/659645.
class RegistryDict {
 public:
  typedef std::map<std::string, RegistryDict*, CaseInsensitiveStringCompare>
      KeyMap;
  typedef std::map<std::string, base::Value*, CaseInsensitiveStringCompare>
      ValueMap;

  RegistryDict();
  ~RegistryDict();

  // Returns a pointer to an existing key, NULL if not present.
  RegistryDict* GetKey(const std::string& name);
  const RegistryDict* GetKey(const std::string& name) const;
  // Sets a key. If |dict| is NULL, clears that key.
  void SetKey(const std::string& name, std::unique_ptr<RegistryDict> dict);
  // Removes a key. If the key doesn't exist, NULL is returned.
  std::unique_ptr<RegistryDict> RemoveKey(const std::string& name);
  // Clears all keys.
  void ClearKeys();

  // Returns a pointer to a value, NULL if not present.
  base::Value* GetValue(const std::string& name);
  const base::Value* GetValue(const std::string& name) const;
  // Sets a value. If |value| is NULL, removes the value.
  void SetValue(const std::string& name, std::unique_ptr<base::Value> value);
  // Removes a value. If the value doesn't exist, NULL is returned.
  std::unique_ptr<base::Value> RemoveValue(const std::string& name);
  // Clears all values.
  void ClearValues();

  // Merge keys and values from |other|, giving precedence to |other|.
  void Merge(const RegistryDict& other);

  // Swap with |other|.
  void Swap(RegistryDict* other);

  const KeyMap& keys() const { return keys_; }
  const ValueMap& values() const { return values_; }

 private:
  KeyMap keys_;
  ValueMap values_;

  DISALLOW_COPY_AND_ASSIGN(RegistryDict);
};

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_REGISTRY_DICT_H_
