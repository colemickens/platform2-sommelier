// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties.h"

#include <string>
#include <vector>

#include "shill/scope_logger.h"

using std::string;
using std::vector;

namespace shill {

// static
bool DBusProperties::GetBool(const DBusPropertiesMap &properties,
                             const string &key,
                             bool *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_bool();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

// static
bool DBusProperties::GetInt32(const DBusPropertiesMap &properties,
                              const string &key,
                              int32 *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_int32();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

// static
bool DBusProperties::GetObjectPath(const DBusPropertiesMap &properties,
                                   const string &key,
                                   string *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_path();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

// static
bool DBusProperties::GetString(const DBusPropertiesMap &properties,
                               const string &key,
                               string *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_string();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

// static
bool DBusProperties::GetStrings(const DBusPropertiesMap &properties,
                                const string &key,
                                vector<string> *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  DBus::MessageIter iter(it->second.reader());
  value->clear();
  iter >> *value;
  SLOG(DBus, 2) << key << " = " ;
  for(vector<string>::const_iterator it = value->begin();
      it != value->end(); ++it)
    SLOG(DBus, 2) << "    " << *it;
  return true;
}

// static
bool DBusProperties::GetUint16(const DBusPropertiesMap &properties,
                               const string &key,
                               uint16 *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_uint16();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

// static
bool DBusProperties::GetUint32(const DBusPropertiesMap &properties,
                               const string &key,
                               uint32 *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_uint32();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

// static
bool DBusProperties::GetRpcIdentifiers(const DBusPropertiesMap &properties,
                                       const string &key,
                                       RpcIdentifiers *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  vector<DBus::Path> paths = it->second.operator vector<DBus::Path>();
  ConvertPathsToRpcIdentifiers(paths, value);
  SLOG(DBus, 2) << key << " = (RpcIdentfier)[" << value->size() << "]";
  return true;
}

// static
void DBusProperties::ConvertPathsToRpcIdentifiers(
    const vector<DBus::Path> &dbus_paths, RpcIdentifiers *rpc_identifiers) {
  rpc_identifiers->clear();
  for (vector<DBus::Path>::const_iterator it = dbus_paths.begin();
       it != dbus_paths.end(); ++it) {
    rpc_identifiers->push_back(*it);
  }
}

// static
std::string DBusProperties::KeysToString(const std::map<std::string,
                                         ::DBus::Variant> &args) {
  std::string keys;
  for (std::map<std::string, ::DBus::Variant>:: const_iterator it =
           args.begin(); it != args.end(); ++it) {
    keys.append(" ");
    keys.append(it->first);
  }
  return keys;
}

}  // namespace shill
