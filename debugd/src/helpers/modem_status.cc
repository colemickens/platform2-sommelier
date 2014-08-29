// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus-c++/dbus.h>
#include <stdio.h>

#include <base/json/json_writer.h>
#include <base/strings/string_util.h>
#include <base/values.h>
#include <chromeos/dbus/service_constants.h>
#include <debugd/src/dbus_utils.h>

#include "dbus_proxies/org.freedesktop.DBus.Properties.h"
#include "dbus_proxies/org.freedesktop.ModemManager.h"
#include "dbus_proxies/org.freedesktop.ModemManager.Modem.h"
#include "dbus_proxies/org.freedesktop.ModemManager.Modem.Simple.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

// These are lifted from modemmanager's XML files, since dbus-c++ currently
// doesn't emit constants for enums defined in headers.
// TODO(ellyjones): fix that
const uint32_t kModemTypeGsm = 1;

class DBusPropertiesProxy : public org::freedesktop::DBus::Properties_proxy,
                            public DBus::ObjectProxy {
 public:
  DBusPropertiesProxy(DBus::Connection& connection, const char* path,  // NOLINT
                      const char* service)
      : DBus::ObjectProxy(connection, path, service) {}
  ~DBusPropertiesProxy() override = default;
};

class ModemManagerProxy : public org::freedesktop::ModemManager_proxy,
                          public DBus::ObjectProxy {
 public:
  ModemManagerProxy(DBus::Connection& connection, const char* path,  // NOLINT
                    const char* service)
      : DBus::ObjectProxy(connection, path, service) {}
  ~ModemManagerProxy() override = default;
  void DeviceAdded(const DBus::Path&) override {}
  void DeviceRemoved(const DBus::Path&) override {}
};

class ModemSimpleProxy
    : public org::freedesktop::ModemManager::Modem::Simple_proxy,
      public DBus::ObjectProxy {
 public:
  ModemSimpleProxy(DBus::Connection& connection, const char* path,  // NOLINT
                   const char* service)
      : DBus::ObjectProxy(connection, path, service) {}
  ~ModemSimpleProxy() override = default;
};

class ModemProxy : public org::freedesktop::ModemManager::Modem_proxy,
                   public DBus::ObjectProxy {
 public:
  ModemProxy(DBus::Connection& connection, const char* path,  // NOLINT
             const char* service)
      : DBus::ObjectProxy(connection, path, service) {}
  ~ModemProxy() override = default;
  void StateChanged(const uint32_t& old_state,
                    const uint32_t& new_state,
                    const uint32_t& reason) override {}
};

struct Modem {
  Modem(const char* service, DBus::Path path)
      : service_(service), path_(path) {}

  Value* GetStatus(DBus::Connection& conn);  // NOLINT

  const char* service_;
  DBus::Path path_;
};

Value* FetchOneInterface(DBusPropertiesProxy& properties,  // NOLINT
                         const char* interface,
                         DictionaryValue* result) {
  std::map<std::string, DBus::Variant> propsmap = properties.GetAll(interface);
  Value* propsdict = NULL;
  if (!debugd::DBusPropertyMapToValue(propsmap, &propsdict))
    return NULL;

  std::string keypath = interface;
  ReplaceSubstringsAfterOffset(&keypath, 0, ".", "/");
  result->Set(keypath, propsdict);
  return propsdict;
}

Value* Modem::GetStatus(DBus::Connection& conn) {  // NOLINT
  DictionaryValue* result = new DictionaryValue();
  result->SetString("service", service_);
  result->SetString("path", path_);

  ModemSimpleProxy simple = ModemSimpleProxy(conn, path_.c_str(), service_);
  Value* status = NULL;
  std::map<std::string, DBus::Variant> statusmap;
  try {
    statusmap = simple.GetStatus();
    // cpplint thinks this is a function call
  } catch(DBus::Error e) {}

  if (debugd::DBusPropertyMapToValue(statusmap, &status))
    result->Set("status", status);

  ModemProxy modem = ModemProxy(conn, path_.c_str(), service_);
  DictionaryValue* infodict = new DictionaryValue();
  try {
    DBus::Struct<std::string,
                 std::string,
                 std::string> infomap = modem.GetInfo();
    infodict->SetString("manufacturer", infomap._1);
    infodict->SetString("modem", infomap._2);
    infodict->SetString("version", infomap._3);
    // cpplint thinks this is a function call
  } catch(DBus::Error e) {}
  result->Set("info", infodict);

  DictionaryValue* props = new DictionaryValue();
  DBusPropertiesProxy properties = DBusPropertiesProxy(conn, path_.c_str(),
                                                       service_);
  FetchOneInterface(properties, cromo::kModemInterface, props);
  FetchOneInterface(properties, cromo::kModemSimpleInterface, props);
  uint32_t type = modem.Type();
  if (type == kModemTypeGsm) {
    FetchOneInterface(properties, cromo::kModemGsmInterface, props);
    FetchOneInterface(properties, cromo::kModemGsmCardInterface, props);
    FetchOneInterface(properties, cromo::kModemGsmNetworkInterface, props);
  } else {
    FetchOneInterface(properties, cromo::kModemCdmaInterface, props);
  }
  result->Set("properties", props);

  return result;
}

int main() {
  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  ModemManagerProxy cromo(conn,
                          cromo::kCromoServicePath,
                          cromo::kCromoServiceName);
  std::vector<Modem> modems;

  // The try-catch block is to account for cromo not being present.
  // We don't want to crash if cromo isn't running, so we swallow the
  // DBus exception we get from the failed attempt to enumerate devices.
  try {
    std::vector<DBus::Path> cromo_modems = cromo.EnumerateDevices();
    for (size_t i = 0; i < cromo_modems.size(); ++i)
      modems.push_back(Modem(cromo::kCromoServiceName, cromo_modems[i]));
    // cpplint thinks this is a function call
  } catch(DBus::Error e) {}

  ListValue result;
  for (size_t i = 0; i < modems.size(); ++i)
    result.Append(modems[i].GetStatus(conn));

  std::string json;
  base::JSONWriter::WriteWithOptions(&result,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
  printf("%s\n", json.c_str());
  return 0;
}
