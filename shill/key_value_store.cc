// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_value_store.h"

#include <base/stl_util.h>

#include "shill/logging.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

KeyValueStore::KeyValueStore() {}

void KeyValueStore::Clear() {
  bool_properties_.clear();
  int_properties_.clear();
  string_properties_.clear();
  stringmap_properties_.clear();
  strings_properties_.clear();
  uint_properties_.clear();
}

bool KeyValueStore::IsEmpty() {
  return
      bool_properties_.empty() &&
      int_properties_.empty() &&
      string_properties_.empty() &&
      stringmap_properties_.empty() &&
      strings_properties_.empty() &&
      uint_properties_.empty();
}

void KeyValueStore::CopyFrom(const KeyValueStore &b) {
  bool_properties_ = b.bool_properties_;
  int_properties_ = b.int_properties_;
  string_properties_ = b.string_properties_;
  stringmap_properties_ = b.stringmap_properties_;
  strings_properties_ = b.strings_properties_;
  uint_properties_ = b.uint_properties_;
}

bool KeyValueStore::Equals(const KeyValueStore &other) const {
  // First compare sizes of all maps to detect unequal stores faster.
  if (bool_properties_.size() != other.bool_properties_.size() ||
      int_properties_.size() != other.int_properties_.size() ||
      string_properties_.size() != other.string_properties_.size() ||
      stringmap_properties_.size() != other.stringmap_properties_.size() ||
      strings_properties_.size()!= other.strings_properties_.size() ||
      uint_properties_.size() != other.uint_properties_.size())
    return false;

  return bool_properties_ == other.bool_properties_ &&
         int_properties_ == other.int_properties_ &&
         string_properties_ == other.string_properties_ &&
         stringmap_properties_ == other.stringmap_properties_ &&
         strings_properties_ == other.strings_properties_ &&
         uint_properties_ == other.uint_properties_;
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

bool KeyValueStore::ContainsStringmap(const std::string &name) const {
  return ContainsKey(stringmap_properties_, name);
}

bool KeyValueStore::ContainsStrings(const string &name) const {
  return ContainsKey(strings_properties_, name);
}

bool KeyValueStore::ContainsUint(const string &name) const {
  return ContainsKey(uint_properties_, name);
}

bool KeyValueStore::GetBool(const string &name) const {
  const auto it(bool_properties_.find(name));
  CHECK(it != bool_properties_.end()) << "for bool property " << name;
  return it->second;
}

int32 KeyValueStore::GetInt(const string &name) const {
  const auto it(int_properties_.find(name));
  CHECK(it != int_properties_.end()) << "for int property " << name;
  return it->second;
}

const string &KeyValueStore::GetString(const string &name) const {
  const auto it(string_properties_.find(name));
  CHECK(it != string_properties_.end()) << "for string property " << name;
  return it->second;
}

const map<string, string> &KeyValueStore::GetStringmap(
    const string &name) const {
  const auto it(stringmap_properties_.find(name));
  CHECK(it != stringmap_properties_.end()) << "for stringmap property "
                                           << name;
  return it->second;
}

const vector<string> &KeyValueStore::GetStrings(const string &name) const {
  const auto it(strings_properties_.find(name));
  CHECK(it != strings_properties_.end()) << "for strings property " << name;
  return it->second;
}

uint32 KeyValueStore::GetUint(const string &name) const {
  const auto it(uint_properties_.find(name));
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

void KeyValueStore::SetStringmap(const string &name,
                                 const map<string, string> &value) {
  stringmap_properties_[name] = value;
}

void KeyValueStore::SetStrings(const string &name,
                               const vector<string> &value) {
  strings_properties_[name] = value;
}

void KeyValueStore::SetUint(const string &name, uint32 value) {
  uint_properties_[name] = value;
}

void KeyValueStore::RemoveString(const string &name) {
  string_properties_.erase(name);
}

void KeyValueStore::RemoveStringmap(const string &name) {
  stringmap_properties_.erase(name);
}

void KeyValueStore::RemoveStrings(const string &name) {
  strings_properties_.erase(name);
}

void KeyValueStore::RemoveInt(const string &name) {
  int_properties_.erase(name);
}

bool KeyValueStore::LookupBool(const string &name, bool default_value) const {
  const auto it(bool_properties_.find(name));
  return it == bool_properties_.end() ? default_value : it->second;
}

int KeyValueStore::LookupInt(const string &name, int default_value) const {
  const auto it(int_properties_.find(name));
  return it == int_properties_.end() ? default_value : it->second;
}

string KeyValueStore::LookupString(const string &name,
                                   const string &default_value) const {
  const auto it(string_properties_.find(name));
  return it == string_properties_.end() ? default_value : it->second;
}

}  // namespace shill
