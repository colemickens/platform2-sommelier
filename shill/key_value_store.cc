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
  byte_arrays_properties_.clear();
  int_properties_.clear();
  int16_properties_.clear();
  key_value_store_properties_.clear();
  rpc_identifier_properties_.clear();
  string_properties_.clear();
  stringmap_properties_.clear();
  strings_properties_.clear();
  uint_properties_.clear();
  uint16_properties_.clear();
  uint8s_properties_.clear();
  uint32s_properties_.clear();
}

bool KeyValueStore::IsEmpty() {
  return
      bool_properties_.empty() &&
      byte_arrays_properties_.empty() &&
      int_properties_.empty() &&
      int16_properties_.empty() &&
      key_value_store_properties_.empty() &&
      rpc_identifier_properties_.empty() &&
      string_properties_.empty() &&
      stringmap_properties_.empty() &&
      strings_properties_.empty() &&
      uint_properties_.empty() &&
      uint16_properties_.empty() &&
      uint8s_properties_.empty() &&
      uint32s_properties_.empty();
}

void KeyValueStore::CopyFrom(const KeyValueStore& b) {
  bool_properties_ = b.bool_properties_;
  byte_arrays_properties_ = b.byte_arrays_properties_;
  int_properties_ = b.int_properties_;
  int16_properties_ = b.int16_properties_;
  key_value_store_properties_ = b.key_value_store_properties_;
  rpc_identifier_properties_ = b.rpc_identifier_properties_;
  string_properties_ = b.string_properties_;
  stringmap_properties_ = b.stringmap_properties_;
  strings_properties_ = b.strings_properties_;
  uint_properties_ = b.uint_properties_;
  uint16_properties_ = b.uint16_properties_;
  uint8s_properties_ = b.uint8s_properties_;
  uint32s_properties_ = b.uint32s_properties_;
}

bool KeyValueStore::KeyValueStorePropertiesAreEqual(
    const KeyValueStore& other) const {
  for (const auto& entry : key_value_store_properties_) {
    const auto& key = entry.first;
    const auto& value = entry.second;
    if (!other.ContainsKeyValueStore(key) ||
        !value.Equals(other.GetKeyValueStore(key))) {
      return false;
    }
  }
  return true;
}

bool KeyValueStore::Equals(const KeyValueStore& other) const {
  // First compare sizes of all maps to detect unequal stores faster.
  if (bool_properties_.size() != other.bool_properties_.size() ||
      byte_arrays_properties_.size() != other.byte_arrays_properties_.size() ||
      int_properties_.size() != other.int_properties_.size() ||
      int16_properties_.size() != other.int16_properties_.size() ||
      key_value_store_properties_.size() !=
          other.key_value_store_properties_.size() ||
      rpc_identifier_properties_.size() !=
          other.rpc_identifier_properties_.size() ||
      string_properties_.size() != other.string_properties_.size() ||
      stringmap_properties_.size() != other.stringmap_properties_.size() ||
      strings_properties_.size()!= other.strings_properties_.size() ||
      uint_properties_.size() != other.uint_properties_.size() ||
      uint16_properties_.size() != other.uint16_properties_.size() ||
      uint8s_properties_.size() != other.uint8s_properties_.size() ||
      uint32s_properties_.size() != other.uint32s_properties_.size())
    return false;

  return bool_properties_ == other.bool_properties_ &&
         byte_arrays_properties_ == other.byte_arrays_properties_ &&
         int_properties_ == other.int_properties_ &&
         int16_properties_ == other.int16_properties_ &&
         KeyValueStorePropertiesAreEqual(other) &&
         rpc_identifier_properties_ == other.rpc_identifier_properties_ &&
         string_properties_ == other.string_properties_ &&
         stringmap_properties_ == other.stringmap_properties_ &&
         strings_properties_ == other.strings_properties_ &&
         uint_properties_ == other.uint_properties_ &&
         uint16_properties_ == other.uint16_properties_ &&
         uint8s_properties_ == other.uint8s_properties_ &&
         uint32s_properties_ == other.uint32s_properties_;
}

bool KeyValueStore::ContainsBool(const string& name) const {
  return ContainsKey(bool_properties_, name);
}

bool KeyValueStore::ContainsByteArrays(const string& name) const {
  return ContainsKey(byte_arrays_properties_, name);
}

bool KeyValueStore::ContainsInt(const string& name) const {
  return ContainsKey(int_properties_, name);
}

bool KeyValueStore::ContainsInt16(const string& name) const {
  return ContainsKey(int16_properties_, name);
}

bool KeyValueStore::ContainsKeyValueStore(const string& name) const {
  return ContainsKey(key_value_store_properties_, name);
}

bool KeyValueStore::ContainsRpcIdentifier(const string& name) const {
  return ContainsKey(rpc_identifier_properties_, name);
}

bool KeyValueStore::ContainsString(const string& name) const {
  return ContainsKey(string_properties_, name);
}

bool KeyValueStore::ContainsStringmap(const std::string& name) const {
  return ContainsKey(stringmap_properties_, name);
}

bool KeyValueStore::ContainsStrings(const string& name) const {
  return ContainsKey(strings_properties_, name);
}

bool KeyValueStore::ContainsUint(const string& name) const {
  return ContainsKey(uint_properties_, name);
}

bool KeyValueStore::ContainsUint16(const string& name) const {
  return ContainsKey(uint16_properties_, name);
}

bool KeyValueStore::ContainsUint8s(const string& name) const {
  return ContainsKey(uint8s_properties_, name);
}

bool KeyValueStore::ContainsUint32s(const string& name) const {
  return ContainsKey(uint32s_properties_, name);
}

bool KeyValueStore::GetBool(const string& name) const {
  const auto it(bool_properties_.find(name));
  CHECK(it != bool_properties_.end()) << "for bool property " << name;
  return it->second;
}

const vector<vector<uint8_t>>& KeyValueStore::GetByteArrays(
    const string& name) const {
  const auto it(byte_arrays_properties_.find(name));
  CHECK(it != byte_arrays_properties_.end())
      << "for byte arrays property " << name;
  return it->second;
}

int32_t KeyValueStore::GetInt(const string& name) const {
  const auto it(int_properties_.find(name));
  CHECK(it != int_properties_.end()) << "for int property " << name;
  return it->second;
}

int16_t KeyValueStore::GetInt16(const string& name) const {
  const auto it(int16_properties_.find(name));
  CHECK(it != int16_properties_.end()) << "for int16 property " << name;
  return it->second;
}

const KeyValueStore& KeyValueStore::GetKeyValueStore(const string& name) const {
  const auto it(key_value_store_properties_.find(name));
  CHECK(it != key_value_store_properties_.end())
      << "for key value store property " << name;
  return it->second;
}

const string& KeyValueStore::GetRpcIdentifier(const string& name) const {
  const auto it(rpc_identifier_properties_.find(name));
  CHECK(it != rpc_identifier_properties_.end())
      << "for rpc identifier property " << name;
  return it->second;
}

const string& KeyValueStore::GetString(const string& name) const {
  const auto it(string_properties_.find(name));
  CHECK(it != string_properties_.end()) << "for string property " << name;
  return it->second;
}

const map<string, string>& KeyValueStore::GetStringmap(
    const string& name) const {
  const auto it(stringmap_properties_.find(name));
  CHECK(it != stringmap_properties_.end()) << "for stringmap property "
                                           << name;
  return it->second;
}

const vector<string>& KeyValueStore::GetStrings(const string& name) const {
  const auto it(strings_properties_.find(name));
  CHECK(it != strings_properties_.end()) << "for strings property " << name;
  return it->second;
}

uint32_t KeyValueStore::GetUint(const string& name) const {
  const auto it(uint_properties_.find(name));
  CHECK(it != uint_properties_.end()) << "for uint property " << name;
  return it->second;
}

uint16_t KeyValueStore::GetUint16(const string& name) const {
  const auto it(uint16_properties_.find(name));
  CHECK(it != uint16_properties_.end()) << "for uint16 property " << name;
  return it->second;
}

const vector<uint8_t>& KeyValueStore::GetUint8s(const string& name) const {
  const auto it(uint8s_properties_.find(name));
  CHECK(it != uint8s_properties_.end())
      << "for uint8s property " << name;
  return it->second;
}

const vector<uint32_t>& KeyValueStore::GetUint32s(const string& name) const {
  const auto it(uint32s_properties_.find(name));
  CHECK(it != uint32s_properties_.end())
      << "for uint32s property " << name;
  return it->second;
}

void KeyValueStore::SetBool(const string& name, bool value) {
  bool_properties_[name] = value;
}

void KeyValueStore::SetByteArrays(const string& name,
                            const vector<vector<uint8_t>>& value) {
  byte_arrays_properties_[name] = value;
}

void KeyValueStore::SetInt(const string& name, int32_t value) {
  int_properties_[name] = value;
}

void KeyValueStore::SetInt16(const string& name, int16_t value) {
  int16_properties_[name] = value;
}

void KeyValueStore::SetKeyValueStore(const string& name,
                                     const KeyValueStore& value) {
  key_value_store_properties_[name].Clear();
  key_value_store_properties_[name].CopyFrom(value);
}

void KeyValueStore::SetRpcIdentifier(const string& name, const string& value) {
  rpc_identifier_properties_[name] = value;
}

void KeyValueStore::SetString(const string& name, const string& value) {
  string_properties_[name] = value;
}

void KeyValueStore::SetStringmap(const string& name,
                                 const map<string, string>& value) {
  stringmap_properties_[name] = value;
}

void KeyValueStore::SetStrings(const string& name,
                               const vector<string>& value) {
  strings_properties_[name] = value;
}

void KeyValueStore::SetUint(const string& name, uint32_t value) {
  uint_properties_[name] = value;
}

void KeyValueStore::SetUint16(const string& name, uint16_t value) {
  uint16_properties_[name] = value;
}

void KeyValueStore::SetUint8s(const string& name,
                              const vector<uint8_t>& value) {
  uint8s_properties_[name] = value;
}

void KeyValueStore::SetUint32s(const string& name,
                               const vector<uint32_t>& value) {
  uint32s_properties_[name] = value;
}

void KeyValueStore::RemoveByteArrays(const string& name) {
  byte_arrays_properties_.erase(name);
}

void KeyValueStore::RemoveInt(const string& name) {
  int_properties_.erase(name);
}

void KeyValueStore::RemoveInt16(const string& name) {
  int16_properties_.erase(name);
}

void KeyValueStore::RemoveKeyValueStore(const string& name) {
  key_value_store_properties_.erase(name);
}

void KeyValueStore::RemoveRpcIdentifier(const string& name) {
  rpc_identifier_properties_.erase(name);
}

void KeyValueStore::RemoveString(const string& name) {
  string_properties_.erase(name);
}

void KeyValueStore::RemoveStringmap(const string& name) {
  stringmap_properties_.erase(name);
}

void KeyValueStore::RemoveStrings(const string& name) {
  strings_properties_.erase(name);
}

void KeyValueStore::RemoveUint16(const string& name) {
  uint16_properties_.erase(name);
}

void KeyValueStore::RemoveUint8s(const string& name) {
  uint8s_properties_.erase(name);
}

void KeyValueStore::RemoveUint32s(const string& name) {
  uint32s_properties_.erase(name);
}

bool KeyValueStore::LookupBool(const string& name, bool default_value) const {
  const auto it(bool_properties_.find(name));
  return it == bool_properties_.end() ? default_value : it->second;
}

int KeyValueStore::LookupInt(const string& name, int default_value) const {
  const auto it(int_properties_.find(name));
  return it == int_properties_.end() ? default_value : it->second;
}

string KeyValueStore::LookupString(const string& name,
                                   const string& default_value) const {
  const auto it(string_properties_.find(name));
  return it == string_properties_.end() ? default_value : it->second;
}

}  // namespace shill
