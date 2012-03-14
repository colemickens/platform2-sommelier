// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_value_store.h"

#include <base/logging.h>
#include <base/stl_util.h>

using std::map;
using std::string;

namespace shill {

KeyValueStore::KeyValueStore() {}

bool KeyValueStore::ContainsBool(const string &name) const {
  return ContainsKey(bool_properties_, name);
}

bool KeyValueStore::ContainsString(const string &name) const {
  return ContainsKey(string_properties_, name);
}

bool KeyValueStore::ContainsUint(const string &name) const {
  return ContainsKey(uint_properties_, name);
}

bool KeyValueStore::GetBool(const string &name) const {
  map<string, bool>::const_iterator it(bool_properties_.find(name));
  CHECK(it != bool_properties_.end()) << "for bool property " << name;
  return it->second;
}

const string &KeyValueStore::GetString(const string &name) const {
  map<string, string>::const_iterator it(string_properties_.find(name));
  CHECK(it != string_properties_.end()) << "for string property " << name;
  return it->second;
}

uint32 KeyValueStore::GetUint(const string &name) const {
  map<string, uint32>::const_iterator it(uint_properties_.find(name));
  CHECK(it != uint_properties_.end()) << "for uint property " << name;
  return it->second;
}

void KeyValueStore::SetBool(const string &name, bool value) {
  bool_properties_[name] = value;
}

void KeyValueStore::SetString(const string &name, const string &value) {
  string_properties_[name] = value;
}

void KeyValueStore::SetUint(const string &name, uint32 value) {
  uint_properties_[name] = value;
}

string KeyValueStore::LookupString(const string &name,
                                   const string &default_value) const {
  return ContainsString(name) ? GetString(name) : default_value;
}

}  // namespace shill
