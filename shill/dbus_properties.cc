// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties.h"

#include <dbus/dbus.h>

#include "shill/logging.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {

template <typename ValueType>
bool GetValue(const DBusPropertiesMap &properties,
              const string &key,
              ValueType *value) {
  CHECK(value);

  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    SLOG(DBus, 2) << "Key '" << key << "' not found.";
    return false;
  }

  string actual_type = it->second.signature();
  string expected_type = DBus::type<ValueType>::sig();
  if (actual_type != expected_type) {
    SLOG(DBus, 2) << "Key '" << key << "' type mismatch (expected '"
                  << expected_type << "', actual '" << actual_type << "').";
    return false;
  }

  *value = it->second.operator ValueType();
  return true;
}

}  // namespace

// static
bool DBusProperties::GetBool(const DBusPropertiesMap &properties,
                             const string &key,
                             bool *value) {
  return GetValue<bool>(properties, key, value);
}

// static
bool DBusProperties::GetDBusPropertiesMap(const DBusPropertiesMap &properties,
                                          const string &key,
                                          DBusPropertiesMap *value) {
  return GetValue<DBusPropertiesMap>(properties, key, value);
}

// static
bool DBusProperties::GetDouble(const DBusPropertiesMap &properties,
                               const string &key,
                               double *value) {
  return GetValue<double>(properties, key, value);
}

// static
bool DBusProperties::GetInt16(const DBusPropertiesMap &properties,
                              const string &key,
                              int16_t *value) {
  return GetValue<int16_t>(properties, key, value);
}

// static
bool DBusProperties::GetInt32(const DBusPropertiesMap &properties,
                              const string &key,
                              int32_t *value) {
  return GetValue<int32_t>(properties, key, value);
}

// static
bool DBusProperties::GetInt64(const DBusPropertiesMap &properties,
                              const string &key,
                              int64_t *value) {
  return GetValue<int64_t>(properties, key, value);
}

// static
bool DBusProperties::GetObjectPath(const DBusPropertiesMap &properties,
                                   const string &key,
                                   DBus::Path *value) {
  return GetValue<DBus::Path>(properties, key, value);
}

// static
bool DBusProperties::GetString(const DBusPropertiesMap &properties,
                               const string &key,
                               string *value) {
  return GetValue<string>(properties, key, value);
}

// static
bool DBusProperties::GetStringmap(const DBusPropertiesMap &properties,
                                  const string &key,
                                  map<string, string> *value) {
  return GetValue<map<string, string>>(properties, key, value);
}

// static
bool DBusProperties::GetStrings(const DBusPropertiesMap &properties,
                                const string &key,
                                vector<string> *value) {
  return GetValue<vector<string>>(properties, key, value);
}

// static
bool DBusProperties::GetUint8(const DBusPropertiesMap &properties,
                               const string &key,
                               uint8_t *value) {
  return GetValue<uint8_t>(properties, key, value);
}

// static
bool DBusProperties::GetUint16(const DBusPropertiesMap &properties,
                               const string &key,
                               uint16_t *value) {
  return GetValue<uint16_t>(properties, key, value);
}

// static
bool DBusProperties::GetUint32(const DBusPropertiesMap &properties,
                               const string &key,
                               uint32_t *value) {
  return GetValue<uint32_t>(properties, key, value);
}

// static
bool DBusProperties::GetUint64(const DBusPropertiesMap &properties,
                               const string &key,
                               uint64_t *value) {
  return GetValue<uint64_t>(properties, key, value);
}

// static
bool DBusProperties::GetRpcIdentifiers(const DBusPropertiesMap &properties,
                                       const string &key,
                                       RpcIdentifiers *value) {
  vector<DBus::Path> paths;
  if (GetValue<vector<DBus::Path>>(properties, key, &paths)) {
    ConvertPathsToRpcIdentifiers(paths, value);
    return true;
  }
  return false;
}

// static
void DBusProperties::ConvertPathsToRpcIdentifiers(
    const vector<DBus::Path> &dbus_paths, RpcIdentifiers *rpc_identifiers) {
  CHECK(rpc_identifiers);
  rpc_identifiers->assign(dbus_paths.begin(), dbus_paths.end());
}

// static
void DBusProperties::ConvertKeyValueStoreToMap(
    const KeyValueStore &store, DBusPropertiesMap *properties) {
  CHECK(properties);
  properties->clear();
  for (const auto &key_value_pair : store.string_properties()) {
    (*properties)[key_value_pair.first].writer()
        .append_string(key_value_pair.second.c_str());
  }
  for (const auto &key_value_pair : store.stringmap_properties()) {
    DBus::MessageIter writer = (*properties)[key_value_pair.first].writer();
    writer << key_value_pair.second;
  }
  for (const auto &key_value_pair : store.strings_properties()) {
    DBus::MessageIter writer = (*properties)[key_value_pair.first].writer();
    writer << key_value_pair.second;
  }
  for (const auto &key_value_pair : store.bool_properties()) {
    (*properties)[key_value_pair.first].writer()
        .append_bool(key_value_pair.second);
  }
  for (const auto &key_value_pair : store.int_properties()) {
    (*properties)[key_value_pair.first].writer()
        .append_int32(key_value_pair.second);
  }
  for (const auto &key_value_pair : store.uint_properties()) {
    (*properties)[key_value_pair.first].writer()
        .append_uint32(key_value_pair.second);
  }
}

// static
string DBusProperties::KeysToString(const DBusPropertiesMap &properties) {
  string keys;
  for (const auto &key_value_pair : properties) {
    keys.append(" ");
    keys.append(key_value_pair.first);
  }
  return keys;
}

}  // namespace shill
