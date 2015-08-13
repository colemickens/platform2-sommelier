// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/json_store.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cinttypes>
#include <map>
#include <memory>
#include <typeinfo>
#include <vector>

#include <base/files/important_file_writer.h>
#include <base/files/file_util.h>
#include <base/json/json_string_value_serializer.h>
#include <base/values.h>

#include "shill/crypto_rot47.h"
#include "shill/logging.h"
#include "shill/scoped_umask.h"

using std::map;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

namespace shill {

namespace Logging {

static auto kModuleLogScope = ScopeLogger::kStorage;
static string ObjectID(const JsonStore* j) {
  return "(unknown)";
}

}  // namespace Logging

namespace {

static const char kRootPropertyDescription[] = "description";
static const char kRootPropertySettings[] = "settings";

bool DoesGroupContainProperties(
    const chromeos::VariantDictionary& group,
    const chromeos::VariantDictionary& required_properties) {
  for (const auto& required_property_name_and_value : required_properties) {
    const auto& required_key = required_property_name_and_value.first;
    const auto& required_value = required_property_name_and_value.second;
    const auto& group_it = group.find(required_key);
    if (group_it == group.end() || group_it->second != required_value) {
      return false;
    }
  }
  return true;
}

}  // namespace

JsonStore::JsonStore() {}

bool JsonStore::IsNonEmpty() const {
  int64_t file_size = 0;
  return base::GetFileSize(path_, &file_size) && file_size != 0;
}

bool JsonStore::Open() {
  CHECK(!path_.empty());
  if (!IsNonEmpty()) {
    LOG(INFO) << "Creating a new key file at |" << path_.value() << "|.";
    return true;
  }

  string json_string;
  if (!base::ReadFileToString(path_, &json_string)) {
    LOG(ERROR) << "Failed to read data from |" << path_.value() << "|.";
    return false;
  }

  JSONStringValueDeserializer json_deserializer(json_string);
  unique_ptr<base::Value> json_value;
  string json_error;
  json_deserializer.set_allow_trailing_comma(true);
  json_value.reset(json_deserializer.Deserialize(nullptr, &json_error));
  if (!json_value) {
    LOG(ERROR) << "Failed to parse JSON data from |" << path_.value() <<"|.";
    SLOG(this, 5) << json_error;
    return false;
  }

  const base::DictionaryValue* root_dictionary;
  if (!json_value->GetAsDictionary(&root_dictionary)) {
    LOG(ERROR) << "JSON value is not a dictionary.";
    return false;
  }

  CHECK(root_dictionary);
  if (root_dictionary->HasKey(kRootPropertyDescription) &&
      !root_dictionary->GetStringWithoutPathExpansion(
          kRootPropertyDescription, &file_description_)) {
    LOG(WARNING) << "Property |" << kRootPropertyDescription
                 << "| is not a string.";
    // Description is non-critical, so continue processing.
  }

  if (!root_dictionary->HasKey(kRootPropertySettings)) {
    LOG(ERROR) << "Property |" << kRootPropertySettings << "| is missing.";
    return false;
  }

  const base::DictionaryValue* settings_dictionary;
  if (!root_dictionary->GetDictionaryWithoutPathExpansion(
          kRootPropertySettings, &settings_dictionary)) {
    LOG(ERROR) << "Property |" << kRootPropertySettings
               << "| is not a dictionary.";
    return false;
  }

  // TODO(quiche): copy data from |settings_dictionary| to
  // |group_name_to_settings_|.

  return true;
}

bool JsonStore::Flush() {
  if (path_.empty()) {
    LOG(ERROR) << "Empty key file path.";
    return false;
  }

  auto groups(make_scoped_ptr(new base::DictionaryValue()));
  // TODO(quiche): Copy |group_name_and_settings_| into |groups|.

  base::DictionaryValue root;
  root.SetStringWithoutPathExpansion(
      kRootPropertyDescription, file_description_);
  root.SetWithoutPathExpansion(kRootPropertySettings, groups.Pass());

  string json_string;
  JSONStringValueSerializer json_serializer(&json_string);
  json_serializer.set_pretty_print(true);
  if (!json_serializer.Serialize(root)) {
    LOG(ERROR) << "Failed to serialize to JSON.";
    return false;
  }

  ScopedUmask owner_only_umask(~(S_IRUSR | S_IWUSR));
  if (!base::ImportantFileWriter::WriteFileAtomically(path_, json_string)) {
    LOG(ERROR) << "Failed to write JSON file: |" << path_.value() << "|.";
    return false;
  }

  return true;
}

set<string> JsonStore::GetGroups() const {
  set<string> matching_groups;
  for (const auto& group_name_and_settings : group_name_to_settings_) {
    matching_groups.insert(group_name_and_settings.first);
  }
  return matching_groups;
}

// Returns a set so that caller can easily test whether a particular group
// is contained within this collection.
set<string> JsonStore::GetGroupsWithKey(const string& key) const {
  set<string> matching_groups;
  // iterate over groups, find ones with matching key
  for (const auto& group_name_and_settings : group_name_to_settings_) {
    const auto& group_name = group_name_and_settings.first;
    const auto& group_settings = group_name_and_settings.second;
    if (group_settings.find(key) != group_settings.end()) {
      matching_groups.insert(group_name);
    }
  }
  return matching_groups;
}

set<string> JsonStore::GetGroupsWithProperties(const KeyValueStore& properties)
    const {
  set<string> matching_groups;
  const chromeos::VariantDictionary& properties_dict(properties.properties());
  for (const auto& group_name_and_settings : group_name_to_settings_) {
    const auto& group_name = group_name_and_settings.first;
    const auto& group_settings = group_name_and_settings.second;
    if (DoesGroupContainProperties(group_settings, properties_dict)) {
      matching_groups.insert(group_name);
    }
  }
  return matching_groups;
}

bool JsonStore::ContainsGroup(const string& group) const {
  const auto& it = group_name_to_settings_.find(group);
  return it != group_name_to_settings_.end();
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
  auto group_name_and_settings = group_name_to_settings_.find(group);
  if (group_name_and_settings != group_name_to_settings_.end()) {
    group_name_to_settings_.erase(group_name_and_settings);
  }
  return true;
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
