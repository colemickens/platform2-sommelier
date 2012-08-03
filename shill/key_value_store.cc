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

void KeyValueStore::Clear() {
  bool_properties_.clear();
  int_properties_.clear();
  string_properties_.clear();
  uint_properties_.clear();
}

bool KeyValueStore::ContainsBool(const string &name) const {
  return ContainsKey(bool_properties_, name);
}

bool KeyValueStore::ContainsInt(const string &name) const {
  return ContainsKey(int_properties_, name);
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

int32 KeyValueStore::GetInt(const string &name) const {
  map<string, int32>::const_iterator it(int_properties_.find(name));
  CHECK(it != int_properties_.end()) << "for int property " << name;
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

void KeyValueStore::SetInt(const string &name, int32 value) {
  int_properties_[name] = value;
}

void KeyValueStore::SetString(const string &name, const string &value) {
  string_properties_[name] = value;
}

void KeyValueStore::SetUint(const string &name, uint32 value) {
  uint_properties_[name] = value;
}

void KeyValueStore::RemoveString(const string &name) {
  string_properties_.erase(name);
}

void KeyValueStore::RemoveInt(const string &name) {
  int_properties_.erase(name);
}

bool KeyValueStore::LookupBool(const string &name, bool default_value) const {
  map<string, bool>::const_iterator it(bool_properties_.find(name));
  return it == bool_properties_.end() ? default_value : it->second;
}

string KeyValueStore::LookupString(const string &name,
                                   const string &default_value) const {
  map<string, string>::const_iterator it(string_properties_.find(name));
  return it == string_properties_.end() ? default_value : it->second;
}

}  // namespace shill
