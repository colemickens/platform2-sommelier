//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/key_value_store.h"

#if defined(ENABLE_BINDER)
#include <binder/PersistableBundle.h>
#include <utils/String16.h>
#endif  // ENABLE_BINDER
#include <base/stl_util.h>

#include "shill/logging.h"

#if defined(ENABLE_BINDER)
using android::os::PersistableBundle;
using android::String16;
#endif  // ENABLE_BINDER
using std::map;
using std::string;
using std::vector;

namespace shill {

KeyValueStore::KeyValueStore() {}

void KeyValueStore::Clear() {
  properties_.clear();
}

bool KeyValueStore::IsEmpty() {
  return properties_.empty();
}

void KeyValueStore::CopyFrom(const KeyValueStore& b) {
  properties_ = b.properties_;
}

bool KeyValueStore::operator==(const KeyValueStore& rhs) const {
  return properties_ == rhs.properties_;
}

bool KeyValueStore::operator!=(const KeyValueStore& rhs) const {
  return properties_ != rhs.properties_;
}

bool KeyValueStore::ContainsBool(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<bool>();
}

bool KeyValueStore::ContainsBools(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<vector<bool>>();
}

bool KeyValueStore::ContainsByteArrays(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second
          .IsTypeCompatible<vector<vector<uint8_t>>>();
}

bool KeyValueStore::ContainsInt(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<int32_t>();
}

bool KeyValueStore::ContainsInts(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<vector<int32_t>>();
}

bool KeyValueStore::ContainsInt16(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<int16_t>();
}

bool KeyValueStore::ContainsInt64(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<int64_t>();
}

bool KeyValueStore::ContainsInt64s(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<vector<int64_t>>();
}

bool KeyValueStore::ContainsDouble(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<double>();
}

bool KeyValueStore::ContainsDoubles(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<vector<double>>();
}

bool KeyValueStore::ContainsKeyValueStore(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<KeyValueStore>();
}

bool KeyValueStore::ContainsRpcIdentifier(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<dbus::ObjectPath>();
}

bool KeyValueStore::ContainsRpcIdentifiers(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second
          .IsTypeCompatible<vector<dbus::ObjectPath>>();
}

bool KeyValueStore::ContainsString(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<string>();
}

bool KeyValueStore::ContainsStringmap(const std::string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<Stringmap>();
}

bool KeyValueStore::ContainsStrings(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<Strings>();
}

bool KeyValueStore::ContainsUint(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<uint32_t>();
}

bool KeyValueStore::ContainsUint8(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<uint8_t>();
}

bool KeyValueStore::ContainsUint16(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<uint16_t>();
}

bool KeyValueStore::ContainsUint8s(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<vector<uint8_t>>();
}

bool KeyValueStore::ContainsUint32s(const string& name) const {
  return ContainsKey(properties_, name) &&
      properties_.find(name)->second.IsTypeCompatible<vector<uint32_t>>();
}

bool KeyValueStore::Contains(const string& name) const {
  return ContainsKey(properties_, name);
}

bool KeyValueStore::GetBool(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<bool>())
      << "for bool property " << name;
  return it->second.Get<bool>();
}

const vector<bool>& KeyValueStore::GetBools(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<vector<bool>>())
      << "for bools property " << name;
  return it->second.Get<vector<bool>>();
}

const vector<vector<uint8_t>>& KeyValueStore::GetByteArrays(
    const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() &&
        it->second.IsTypeCompatible<vector<vector<uint8_t>>>())
      << "for byte arrays property " << name;
  return it->second.Get<vector<vector<uint8_t>>>();
}

int32_t KeyValueStore::GetInt(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<int32_t>())
      << "for int property " << name;
  return it->second.Get<int32_t>();
}

const vector<int32_t>& KeyValueStore::GetInts(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() &&
        it->second.IsTypeCompatible<vector<int32_t>>())
      << "for ints property " << name;
  return it->second.Get<vector<int32_t>>();
}

int16_t KeyValueStore::GetInt16(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<int16_t>())
      << "for int16 property " << name;
  return it->second.Get<int16_t>();
}

int64_t KeyValueStore::GetInt64(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<int64_t>())
      << "for int64 property " << name;
  return it->second.Get<int64_t>();
}

const vector<int64_t>& KeyValueStore::GetInt64s(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() &&
        it->second.IsTypeCompatible<vector<int64_t>>())
      << "for int64s property " << name;
  return it->second.Get<vector<int64_t>>();
}

double KeyValueStore::GetDouble(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<double>())
      << "for double property " << name;
  return it->second.Get<double>();
}

const vector<double>& KeyValueStore::GetDoubles(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() &&
        it->second.IsTypeCompatible<vector<double>>())
      << "for doubles property " << name;
  return it->second.Get<vector<double>>();
}

const KeyValueStore& KeyValueStore::GetKeyValueStore(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<KeyValueStore>())
      << "for key value store property " << name;
  return it->second.Get<KeyValueStore>();
}

const string& KeyValueStore::GetRpcIdentifier(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() &&
        it->second.IsTypeCompatible<dbus::ObjectPath>())
      << "for rpc identifier property " << name;
  return it->second.Get<dbus::ObjectPath>().value();
}

vector<string> KeyValueStore::GetRpcIdentifiers(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() &&
        it->second.IsTypeCompatible<vector<dbus::ObjectPath>>())
      << "for rpc identifier property " << name;
  RpcIdentifiers ids;
  KeyValueStore::ConvertPathsToRpcIdentifiers(
      it->second.Get<vector<dbus::ObjectPath>>(), &ids);
  return ids;
}

const string& KeyValueStore::GetString(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<string>())
      << "for string property " << name;
  return it->second.Get<string>();
}

const map<string, string>& KeyValueStore::GetStringmap(
    const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<Stringmap>())
      << "for stringmap property " << name;
  return it->second.Get<Stringmap>();
}

const vector<string>& KeyValueStore::GetStrings(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<Strings>())
      << "for strings property " << name;
  return it->second.Get<Strings>();
}

uint32_t KeyValueStore::GetUint(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<uint32_t>())
      << "for uint32 property " << name;
  return it->second.Get<uint32_t>();
}

uint16_t KeyValueStore::GetUint16(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<uint16_t>())
      << "for uint16 property " << name;
  return it->second.Get<uint16_t>();
}

uint8_t KeyValueStore::GetUint8(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() && it->second.IsTypeCompatible<uint8_t>())
      << "for uint8 property " << name;
  return it->second.Get<uint8_t>();
}

const vector<uint8_t>& KeyValueStore::GetUint8s(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() &&
        it->second.IsTypeCompatible<vector<uint8_t>>())
      << "for uint8s property " << name;
  return it->second.Get<vector<uint8_t>>();
}

const vector<uint32_t>& KeyValueStore::GetUint32s(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end() &&
        it->second.IsTypeCompatible<vector<uint32_t>>())
      << "for uint32s property " << name;
  return it->second.Get<vector<uint32_t>>();
}

const brillo::Any& KeyValueStore::Get(const string& name) const {
  const auto it(properties_.find(name));
  CHECK(it != properties_.end());
  return it->second;
}

void KeyValueStore::SetBool(const string& name, bool value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetBools(const string& name, const vector<bool>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetByteArrays(const string& name,
                                  const vector<vector<uint8_t>>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetInt(const string& name, int32_t value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetInts(const string& name, const vector<int32_t>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetInt16(const string& name, int16_t value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetInt64(const string& name, int64_t value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetInt64s(const string& name,
                              const vector<int64_t>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetDouble(const string& name, double value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetDoubles(const string& name,
                               const vector<double>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetKeyValueStore(const string& name,
                                     const KeyValueStore& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetRpcIdentifier(const string& name, const string& value) {
  properties_[name] = brillo::Any(dbus::ObjectPath(value));
}

void KeyValueStore::SetRpcIdentifiers(const string& name,
                                      const vector<string>& value) {
  vector<dbus::ObjectPath> paths;
  for (const auto& rpcid : value) {
    paths.push_back(dbus::ObjectPath(rpcid));
  }
  properties_[name] = brillo::Any(paths);
}

void KeyValueStore::SetString(const string& name, const string& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetStringmap(const string& name,
                                 const map<string, string>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetStrings(const string& name,
                               const vector<string>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetUint(const string& name, uint32_t value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetUint16(const string& name, uint16_t value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetUint8(const string& name, uint8_t value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetUint8s(const string& name,
                              const vector<uint8_t>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::SetUint32s(const string& name,
                               const vector<uint32_t>& value) {
  properties_[name] = brillo::Any(value);
}

void KeyValueStore::Set(const string& name, const brillo::Any& value) {
  properties_[name] = value;
}

void KeyValueStore::RemoveByteArrays(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveInt(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveInt16(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveKeyValueStore(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveRpcIdentifier(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveString(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveStringmap(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveStrings(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveUint16(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveUint8(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveUint8s(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::RemoveUint32s(const string& name) {
  properties_.erase(name);
}

void KeyValueStore::Remove(const string& name) {
  properties_.erase(name);
}

bool KeyValueStore::LookupBool(const string& name, bool default_value) const {
  const auto it(properties_.find(name));
  if (it == properties_.end()) {
    return default_value;
  }
  CHECK(it->second.IsTypeCompatible<bool>()) << "type mismatched";
  return it->second.Get<bool>();
}

int KeyValueStore::LookupInt(const string& name, int default_value) const {
  const auto it(properties_.find(name));
  if (it == properties_.end()) {
    return default_value;
  }
  CHECK(it->second.IsTypeCompatible<int32_t>()) << "type mismatched";
  return it->second.Get<int32_t>();
}

string KeyValueStore::LookupString(const string& name,
                                   const string& default_value) const {
  const auto it(properties_.find(name));
  if (it == properties_.end()) {
    return default_value;
  }
  CHECK(it->second.IsTypeCompatible<string>()) << "type mismatched";
  return it->second.Get<string>();
}

// static.
void KeyValueStore::ConvertToVariantDictionary(
    const KeyValueStore& in_store, brillo::VariantDictionary* out_dict) {
  for (const auto& key_value_pair : in_store.properties_) {
    if (key_value_pair.second.IsTypeCompatible<KeyValueStore>()) {
      // Special handling for nested KeyValueStore (convert it to
      // nested brillo::VariantDictionary).
      brillo::VariantDictionary dict;
      ConvertToVariantDictionary(
          key_value_pair.second.Get<KeyValueStore>(), &dict);
      out_dict->emplace(key_value_pair.first, dict);
    } else {
      out_dict->insert(key_value_pair);
    }
  }
}

// static.
void KeyValueStore::ConvertFromVariantDictionary(
    const brillo::VariantDictionary& in_dict, KeyValueStore* out_store) {
  for (const auto& key_value_pair : in_dict) {
    if (key_value_pair.second.IsTypeCompatible<brillo::VariantDictionary>()) {
      // Special handling for nested brillo::VariantDictionary (convert it to
      // nested KeyValueStore).
      KeyValueStore store;
      ConvertFromVariantDictionary(
          key_value_pair.second.Get<brillo::VariantDictionary>(), &store);
      out_store->properties_.emplace(key_value_pair.first, store);
    } else {
      out_store->properties_.insert(key_value_pair);
    }
  }
}

#if defined(ENABLE_BINDER)
// static.
void KeyValueStore::ConvertToPersistableBundle(const KeyValueStore& in_store,
                                               PersistableBundle* out_bundle) {
  for (const auto& key_value_pair : in_store.properties()) {
    if (key_value_pair.second.IsTypeCompatible<bool>()) {
      out_bundle->putBoolean(String16(key_value_pair.first.c_str()),
                             key_value_pair.second.Get<bool>());
    } else if (key_value_pair.second.IsTypeCompatible<int32_t>()) {
      out_bundle->putInt(String16(key_value_pair.first.c_str()),
                         key_value_pair.second.Get<int32_t>());
    } else if (key_value_pair.second.IsTypeCompatible<int64_t>()) {
      out_bundle->putLong(String16(key_value_pair.first.c_str()),
                          key_value_pair.second.Get<int64_t>());
    } else if (key_value_pair.second.IsTypeCompatible<double>()) {
      out_bundle->putDouble(String16(key_value_pair.first.c_str()),
                            key_value_pair.second.Get<double>());
    } else if (key_value_pair.second.IsTypeCompatible<string>()) {
      out_bundle->putString(
          String16(key_value_pair.first.c_str()),
          String16(key_value_pair.second.Get<string>().c_str()));
    } else if (key_value_pair.second.IsTypeCompatible<vector<bool>>()) {
      out_bundle->putBooleanVector(String16(key_value_pair.first.c_str()),
                                   key_value_pair.second.Get<vector<bool>>());
    } else if (key_value_pair.second.IsTypeCompatible<vector<int32_t>>()) {
      out_bundle->putIntVector(String16(key_value_pair.first.c_str()),
                               key_value_pair.second.Get<vector<int32_t>>());
    } else if (key_value_pair.second.IsTypeCompatible<vector<int64_t>>()) {
      out_bundle->putLongVector(String16(key_value_pair.first.c_str()),
                                key_value_pair.second.Get<vector<int64_t>>());
    } else if (key_value_pair.second.IsTypeCompatible<vector<double>>()) {
      out_bundle->putDoubleVector(String16(key_value_pair.first.c_str()),
                                  key_value_pair.second.Get<vector<double>>());
    } else if (key_value_pair.second.IsTypeCompatible<vector<string>>()) {
      vector<String16> string16_vector;
      for (const string& std_string :
           key_value_pair.second.Get<vector<string>>()) {
        string16_vector.emplace_back(String16(std_string.c_str()));
      }
      out_bundle->putStringVector(String16(key_value_pair.first.c_str()),
                                  string16_vector);
    } else if (key_value_pair.second.IsTypeCompatible<KeyValueStore>()) {
      PersistableBundle bundle;
      ConvertToPersistableBundle(key_value_pair.second.Get<KeyValueStore>(),
                                 &bundle);
      out_bundle->putPersistableBundle(String16(key_value_pair.first.c_str()),
                                       bundle);
    } else {
      LOG(ERROR) << __func__ << ": KeyValueStore entry with key "
                 << key_value_pair.first
                 << " contains a value type that is not supported by "
                    "PersistableBundle";
    }
  }
}

// static.
void KeyValueStore::ConvertFromPersistableBundle(
    const PersistableBundle& in_bundle, KeyValueStore* out_store) {
  if (in_bundle.empty()) {
    return;
  }

  bool bool_value;
  for (const auto& key : in_bundle.getBooleanKeys()) {
    in_bundle.getBoolean(key, &bool_value);
    out_store->SetBool(String16::std_string(key), bool_value);
  }

  int32_t int_value;
  for (const auto& key : in_bundle.getIntKeys()) {
    in_bundle.getInt(key, &int_value);
    out_store->SetInt(String16::std_string(key), int_value);
  }

  int64_t long_value;
  for (const auto& key : in_bundle.getLongKeys()) {
    in_bundle.getLong(key, &long_value);
    out_store->SetInt64(String16::std_string(key), long_value);
  }

  double double_value;
  for (const auto& key : in_bundle.getDoubleKeys()) {
    in_bundle.getDouble(key, &double_value);
    out_store->SetDouble(String16::std_string(key), double_value);
  }

  String16 string_value;
  for (const auto& key : in_bundle.getStringKeys()) {
    in_bundle.getString(key, &string_value);
    out_store->SetString(String16::std_string(key),
                         String16::std_string(string_value));
  }

  vector<bool> bool_vector_value;
  for (const auto& key : in_bundle.getBooleanVectorKeys()) {
    in_bundle.getBooleanVector(key, &bool_vector_value);
    out_store->SetBools(String16::std_string(key), bool_vector_value);
  }

  vector<int32_t> int_vector_value;
  for (const auto& key : in_bundle.getIntVectorKeys()) {
    in_bundle.getIntVector(key, &int_vector_value);
    out_store->SetInts(String16::std_string(key), int_vector_value);
  }

  vector<int64_t> long_vector_value;
  for (const auto& key : in_bundle.getLongVectorKeys()) {
    in_bundle.getLongVector(key, &long_vector_value);
    out_store->SetInt64s(String16::std_string(key), long_vector_value);
  }

  vector<double> double_vector_value;
  for (const auto& key : in_bundle.getDoubleVectorKeys()) {
    in_bundle.getDoubleVector(key, &double_vector_value);
    out_store->SetDoubles(String16::std_string(key), double_vector_value);
  }

  vector<String16> string16_vector_value;
  for (const auto& key : in_bundle.getStringVectorKeys()) {
    in_bundle.getStringVector(key, &string16_vector_value);
    vector<string> string_vector_value;
    for (const String16& string16 : string16_vector_value) {
      string_vector_value.emplace_back(String16::std_string(string16));
    }
    out_store->SetStrings(String16::std_string(key), string_vector_value);
  }

  PersistableBundle persistable_bundle_value;
  for (const auto& key : in_bundle.getPersistableBundleKeys()) {
    in_bundle.getPersistableBundle(key, &persistable_bundle_value);
    KeyValueStore key_value_store_value;
    ConvertFromPersistableBundle(persistable_bundle_value,
                                 &key_value_store_value);
    out_store->SetKeyValueStore(String16::std_string(key),
                                key_value_store_value);
  }
}
#endif  // ENABLE_BINDER

// static.
void KeyValueStore::ConvertPathsToRpcIdentifiers(
  const vector<dbus::ObjectPath>& paths, vector<string>* rpc_identifiers) {
  for (const auto& path : paths) {
    rpc_identifiers->push_back(path.value());
  }
}

}  // namespace shill
