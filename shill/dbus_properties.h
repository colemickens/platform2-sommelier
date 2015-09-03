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

#ifndef SHILL_DBUS_PROPERTIES_H_
#define SHILL_DBUS_PROPERTIES_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <dbus-c++/types.h>

#include "shill/accessor_interface.h"
#include "shill/dbus_variant_gmock_printer.h"
#include "shill/error.h"

namespace shill {

typedef std::map<std::string, DBus::Variant> DBusPropertiesMap;
typedef std::map<uint32_t, DBus::Variant> DBusEnumValueMap;

class DBusProperties {
 public:
  static bool GetBool(const DBusPropertiesMap& properties,
                      const std::string& key,
                      bool* value);

  static bool GetByteArrays(const DBusPropertiesMap& properties,
                            const std::string& key,
                            std::vector<std::vector<uint8_t>>* value);

  static bool GetDBusPropertiesMap(const DBusPropertiesMap& properties,
                                   const std::string& key,
                                   DBusPropertiesMap* value);

  static bool GetDouble(const DBusPropertiesMap& properties,
                        const std::string& key,
                        double* value);

  static bool GetInt16(const DBusPropertiesMap& properties,
                       const std::string& key,
                       int16_t* value);

  static bool GetInt32(const DBusPropertiesMap& properties,
                       const std::string& key,
                       int32_t* value);

  static bool GetInt64(const DBusPropertiesMap& properties,
                       const std::string& key,
                       int64_t* value);

  static bool GetObjectPath(const DBusPropertiesMap& properties,
                            const std::string& key,
                            DBus::Path* value);

  static bool GetString(const DBusPropertiesMap& properties,
                        const std::string& key,
                        std::string* value);

  static bool GetStringmap(const DBusPropertiesMap& properties,
                           const std::string& key,
                           std::map<std::string, std::string>* value);

  static bool GetStrings(const DBusPropertiesMap& properties,
                         const std::string& key,
                         std::vector<std::string>* value);

  static bool GetUint8(const DBusPropertiesMap& properties,
                       const std::string& key,
                       uint8_t* value);

  static bool GetUint16(const DBusPropertiesMap& properties,
                        const std::string& key,
                        uint16_t* value);

  static bool GetUint32(const DBusPropertiesMap& properties,
                        const std::string& key,
                        uint32_t* value);

  static bool GetUint64(const DBusPropertiesMap& properties,
                        const std::string& key,
                        uint64_t* value);
  static bool GetUint8s(const DBusPropertiesMap& properties,
                        const std::string& key,
                        std::vector<uint8_t>* value);

  static bool GetUint32s(const DBusPropertiesMap& properties,
                         const std::string& key,
                         std::vector<uint32_t>* value);

  static bool GetRpcIdentifiers(const DBusPropertiesMap& properties,
                                const std::string& key,
                                RpcIdentifiers* value);

  static void ConvertPathsToRpcIdentifiers(
      const std::vector<DBus::Path>& dbus_paths,
      RpcIdentifiers* rpc_identifiers);

  static void ConvertKeyValueStoreToMap(
      const KeyValueStore& store, DBusPropertiesMap* properties);
  static void ConvertMapToKeyValueStore(
      const DBusPropertiesMap& properties, KeyValueStore* store, Error* error);

  static std::string KeysToString(const DBusPropertiesMap& properties);

 private:
  DISALLOW_COPY_AND_ASSIGN(DBusProperties);
};

}  // namespace shill

#endif  // SHILL_DBUS_PROPERTIES_H_
