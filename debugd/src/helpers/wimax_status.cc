// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <memory>
#include <utility>

#include <base/json/json_writer.h>
#include <base/memory/ptr_util.h>
#include <base/values.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>
#include <debugd/src/dbus_utils.h>

#include "dbus_proxies/org.freedesktop.DBus.Properties.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using std::map;
using std::string;

class DBusPropertiesProxy : public org::freedesktop::DBus::Properties_proxy,
                            public DBus::ObjectProxy {
 public:
  DBusPropertiesProxy(DBus::Connection* connection,
                      const string& path,
                      const string& service)
      : DBus::ObjectProxy(*connection, path, service.c_str()) {}

  ~DBusPropertiesProxy() override = default;
};

class WiMaxStatus {
 public:
  explicit WiMaxStatus(DBus::Connection* connection)
      : connection_(connection) {}

  ~WiMaxStatus() = default;

  string GetJsonOutput() {
    string json;
    std::unique_ptr<DictionaryValue> manager_properties(GetManagerProperties());
    if (manager_properties) {
      base::JSONWriter::WriteWithOptions(
          *manager_properties, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
    }
    return json;
  }

 private:
  std::unique_ptr<DictionaryValue> GetDBusProperties(const string& path,
                                                     const string& service,
                                                     const string& interface) {
    DBusPropertiesProxy properties_proxy(connection_, path, service);
    map<string, DBus::Variant> properties_map;
    try {
      properties_map = properties_proxy.GetAll(interface);
    } catch (const DBus::Error& error) {
      return nullptr;
    }

    std::unique_ptr<Value> properties_value;
    if (!debugd::DBusPropertyMapToValue(properties_map, &properties_value))
      return nullptr;

    return DictionaryValue::From(std::move(properties_value));
  }

  std::unique_ptr<DictionaryValue> ExpandNetworkPathsToProperties(
      std::unique_ptr<DictionaryValue> device_properties) {
    auto networks = base::MakeUnique<ListValue>();
    const ListValue* network_paths = nullptr;
    if (device_properties->GetList(wimax_manager::kNetworksProperty,
                                   &network_paths)) {
      for (size_t i = 0; i < network_paths->GetSize(); ++i) {
        string network_path;
        if (!network_paths->GetString(i, &network_path))
          continue;

        auto network_properties =
            GetDBusProperties(network_path,
                              wimax_manager::kWiMaxManagerServiceName,
                              wimax_manager::kWiMaxManagerNetworkInterface);
        if (!network_properties)
          continue;

        networks->Append(std::move(network_properties));
      }
    }
    device_properties->Set(wimax_manager::kNetworksProperty,
                           std::move(networks));
    return device_properties;
  }

  std::unique_ptr<DictionaryValue> ExpandDevicePathsToProperties(
      std::unique_ptr<DictionaryValue> manager_properties) {
    auto devices = base::MakeUnique<ListValue>();
    const ListValue* device_paths = nullptr;
    if (manager_properties->GetList(wimax_manager::kDevicesProperty,
                                    &device_paths)) {
      for (size_t i = 0; i < device_paths->GetSize(); ++i) {
        string device_path;
        if (!device_paths->GetString(i, &device_path))
          continue;

        auto device_properties =
            GetDBusProperties(device_path,
                              wimax_manager::kWiMaxManagerServiceName,
                              wimax_manager::kWiMaxManagerDeviceInterface);
        if (!device_properties)
          continue;

        device_properties =
            ExpandNetworkPathsToProperties(std::move(device_properties));
        devices->Append(std::move(device_properties));
      }
    }
    manager_properties->Set(wimax_manager::kDevicesProperty,
                            std::move(devices));
    return manager_properties;
  }

  std::unique_ptr<DictionaryValue> GetManagerProperties() {
    auto manager_properties =
        GetDBusProperties(wimax_manager::kWiMaxManagerServicePath,
                          wimax_manager::kWiMaxManagerServiceName,
                          wimax_manager::kWiMaxManagerInterface);
    if (manager_properties) {
      manager_properties =
          ExpandDevicePathsToProperties(std::move(manager_properties));
    }
    return manager_properties;
  }

 private:
  DBus::Connection* connection_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxStatus);
};

int main() {
  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection connection = DBus::Connection::SystemBus();
  WiMaxStatus status(&connection);
  printf("%s\n", status.GetJsonOutput().c_str());
  return 0;
}
