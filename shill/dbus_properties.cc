// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties.h"

#include "shill/scope_logger.h"

namespace shill {

// static
bool DBusProperties::GetString(const DBusPropertiesMap &properties,
                               const std::string &key,
                               std::string *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_string();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

// static
bool DBusProperties::GetObjectPath(const DBusPropertiesMap &properties,
                                   const std::string &key,
                                   std::string *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_path();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

// static
bool DBusProperties::GetUint32(const DBusPropertiesMap &properties,
                               const std::string &key,
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
bool DBusProperties::GetUint16(const DBusPropertiesMap &properties,
                               const std::string &key,
                               uint16 *value) {
  DBusPropertiesMap::const_iterator it = properties.find(key);
  if (it == properties.end()) {
    return false;
  }
  *value = it->second.reader().get_uint16();
  SLOG(DBus, 2) << key << " = " << *value;
  return true;
}

}  // namespace shill
