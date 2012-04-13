// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_PROPERTIES_
#define SHILL_DBUS_PROPERTIES_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/types.h>

namespace shill {

typedef std::map<std::string, DBus::Variant> DBusPropertiesMap;

class DBusProperties {
 public:
  static bool GetBool(const DBusPropertiesMap &properties,
                      const std::string &key,
                      bool *value);

  static bool GetInt32(const DBusPropertiesMap &properties,
                       const std::string &key,
                       int32 *value);

  static bool GetObjectPath(const DBusPropertiesMap &properties,
                            const std::string &key,
                            std::string *value);

  static bool GetString(const DBusPropertiesMap &properties,
                        const std::string &key,
                        std::string *value);

  static bool GetUint16(const DBusPropertiesMap &properties,
                        const std::string &key,
                        uint16 *value);

  static bool GetUint32(const DBusPropertiesMap &properties,
                        const std::string &key,
                        uint32 *value);

 private:
  DISALLOW_COPY_AND_ASSIGN(DBusProperties);
};

}  // namespace shill

#endif  // SHILL_DBUS_PROPERTIES_
