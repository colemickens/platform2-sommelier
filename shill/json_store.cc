// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/json_store.h"

#include <cinttypes>
#include <map>
#include <memory>
#include <typeinfo>
#include <vector>

#include "shill/crypto_rot47.h"
#include "shill/logging.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

namespace Logging {

static auto kModuleLogScope = ScopeLogger::kStorage;
static string ObjectID(const JsonStore* j) {
  return "(unknown)";
}

}  // namespace Logging

JsonStore::JsonStore() {}

bool JsonStore::Flush() {
  NOTIMPLEMENTED();
  return false;
}

set<string> JsonStore::GetGroups() const {
  NOTIMPLEMENTED();
  return set<string>();
}

// Returns a set so that caller can easily test whether a particular group
// is contained within this collection.
set<string> JsonStore::GetGroupsWithKey(const string& key) const {
  NOTIMPLEMENTED();
  return set<string>();
}

set<string> JsonStore::GetGroupsWithProperties(const KeyValueStore& properties)
    const {
  NOTIMPLEMENTED();
  return set<string>();
}

bool JsonStore::ContainsGroup(const string& group) const {
  NOTIMPLEMENTED();
  return false;
}

bool JsonStore::DeleteKey(const string& group, const string& key) {
  const auto& group_name_and_settings = group_name_to_settings_.find(group);
  if (group_name_and_settings == group_name_to_settings_.end()) {
    LOG(ERROR) << "Could not find group |" << group << "|.";
    return false;
  }

  auto& group_settings = group_name_and_settings->second;
  auto property_it = group_settings.find(key);
  if (property_it != group_settings.end()) {
    group_settings.erase(property_it);
  }

  return true;
}

bool JsonStore::DeleteGroup(const string& group) {
  NOTIMPLEMENTED();
  return false;
}

bool JsonStore::SetHeader(const string& header) {
  NOTIMPLEMENTED();
  return false;
}

bool JsonStore::GetString(const string& group,
                          const string& key,
                          string* value) const {
  return ReadSetting(group, key, value);
}

bool JsonStore::SetString(
    const string& group, const string& key, const string& value) {
  return WriteSetting(group, key, value);
}

bool JsonStore::GetBool(const string& group, const string& key, bool* value)
    const {
  return ReadSetting(group, key, value);
}

bool JsonStore::SetBool(const string& group, const string& key, bool value) {
  return WriteSetting(group, key, value);
}

bool JsonStore::GetInt(
    const string& group, const string& key, int* value) const {
  return ReadSetting(group, key, value);
}

bool JsonStore::SetInt(const string& group, const string& key, int value) {
  return WriteSetting(group, key, value);
}

bool JsonStore::GetUint64(
    const string& group, const string& key, uint64_t* value) const {
  return ReadSetting(group, key, value);
}

bool JsonStore::SetUint64(
    const string& group, const string& key, uint64_t value) {
  return WriteSetting(group, key, value);
}

bool JsonStore::GetStringList(
    const string& group, const string& key, vector<string>* value) const {
  return ReadSetting(group, key, value);
}

bool JsonStore::SetStringList(
    const string& group, const string& key, const vector<string>& value) {
  return WriteSetting(group, key, value);
}

bool JsonStore::GetCryptedString(
    const string& group, const string& key, string* value) {
  string encrypted_value;
  if (!GetString(group, key, &encrypted_value)) {
    return false;
  }

  // TODO(quiche): Once we've removed the glib dependency in
  // CryptoProvider, move to using CryptoProvider, instead of
  // CryptoROT47 directly. This change should be done before using
  // JsonStore in production, as the on-disk format of crypted strings
  // will change.
  CryptoROT47 rot47;
  string decrypted_value;
  if (!rot47.Decrypt(encrypted_value, &decrypted_value)) {
    LOG(ERROR) << "Failed to decrypt value for |" << group << "|"
               << ":|" << key << "|.";
    return false;
  }

  if (value) {
    *value = decrypted_value;
  }
  return true;
}

bool JsonStore::SetCryptedString(
    const string& group, const string& key, const string& value) {
  CryptoROT47 rot47;
  string encrypted_value;
  if (!rot47.Encrypt(value, &encrypted_value)) {
    LOG(ERROR) << "Failed to encrypt value for |" << group << "|"
               << ":|" << key << "|.";
    return false;
  }

  return SetString(group, key, encrypted_value);
}

// Private methods.
template<typename T>
bool JsonStore::ReadSetting(
    const string& group, const string& key, T* out) const {
  const auto& group_name_and_settings = group_name_to_settings_.find(group);
  if (group_name_and_settings == group_name_to_settings_.end()) {
    SLOG(this, 10) << "Could not find group |" << group << "|.";
    return false;
  }

  const auto& group_settings = group_name_and_settings->second;
  const auto& property_name_and_value = group_settings.find(key);
  if (property_name_and_value == group_settings.end()) {
    SLOG(this, 10) << "Could not find property |" << key << "|.";
    return false;
  }

  const auto& desired_type = typeid(*out);
  const auto& available_type = property_name_and_value->second.GetType();
  if (available_type != desired_type) {
    // We assume that the reader and the writer agree on the exact
    // type. So we do not allow implicit conversion.
    LOG(ERROR) << "Can not read |" << desired_type.name() << "| from |"
               << available_type.name() << "|.";
    return false;
  }

  if (out) {
    return property_name_and_value->second.GetValue(out);
  } else {
    return true;
  }
}

template<typename T>
bool JsonStore::WriteSetting(
    const string& group, const string& key, const T& new_value) {
  auto group_name_and_settings = group_name_to_settings_.find(group);
  if (group_name_and_settings == group_name_to_settings_.end()) {
    group_name_to_settings_[group][key] = new_value;
    return true;
  }

  auto& group_settings = group_name_and_settings->second;
  auto property_name_and_value = group_settings.find(key);
  if (property_name_and_value == group_settings.end()) {
    group_settings[key] = new_value;
    return true;
  }

  const auto& new_type = typeid(new_value);
  const auto& current_type = property_name_and_value->second.GetType();
  if (new_type != current_type) {
    SLOG(this, 10) << "New type |" << new_type.name()
                   << "| differs from current type |" << current_type.name()
                   << "|.";
    return false;
  } else {
    property_name_and_value->second = new_value;
    return true;
  }
}

}  // namespace shill
