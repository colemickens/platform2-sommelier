// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus-c++/dbus.h>
#include <stdio.h>

#include <base/json/json_writer.h>
#include <base/string_util.h>
#include <base/values.h>
#include <chromeos/utility.h>

#include "proxies/org.freedesktop.DBus.Properties.h"
#include "proxies/org.freedesktop.ModemManager.h"
#include "proxies/org.freedesktop.ModemManager.Modem.h"
#include "proxies/org.freedesktop.ModemManager.Modem.Simple.h"

// These are lifted from modemmanager's XML files, since dbus-c++ currently
// doesn't emit constants for enums defined in headers.
// TODO(ellyjones): fix that
const uint32_t kModemTypeGsm = 1;
const uint32_t kModemTypeCdma = 2;

const char* kMMPath = "/org/freedesktop/ModemManager";
const char* kMMService = "org.freedesktop.ModemManager";

const char* kCromoPath = "/org/chromium/ModemManager";
const char* kCromoService = "org.chromium.ModemManager";

const char* kModemInterface = "org.freedesktop.ModemManager.Modem";
const char* kModemSimpleInterface = "org.freedesktop.ModemManager.Modem.Simple";
const char* kModemCdmaInterface = "org.freedesktop.ModemManager.Modem.Cdma";
const char* kModemGsmInterface = "org.freedesktop.ModemManager.Modem.Gsm";
const char* kModemGsmCardInterface =
    "org.freedesktop.ModemManager.Modem.Gsm.Card";
const char* kModemGsmNetworkInterface =
    "org.freedesktop.ModemManager.Modem.Gsm.Network";
const char* kModemGobiInterface =
    "org.chromium.ModemManager.Modem.Gobi";

class DBusPropertiesProxy
    : public org::freedesktop::DBus::Properties_proxy,
      public DBus::ObjectProxy {
 public:
  DBusPropertiesProxy(DBus::Connection& connection, const char* path, // NOLINT
                      const char* service) :
      DBus::ObjectProxy(connection, path, service) { }
  virtual ~DBusPropertiesProxy() { }
  virtual void PropertiesChanged(const std::string&,
                                 const std::map<std::string, DBus::Variant>&,
                                 const std::vector<std::string>&) { }
};

class ModemManagerProxy
    : public org::freedesktop::ModemManager_proxy,
      public DBus::ObjectProxy {
 public:
  ModemManagerProxy(DBus::Connection& connection, const char* path, // NOLINT
                    const char* service) :
      DBus::ObjectProxy(connection, path, service) { }
  virtual ~ModemManagerProxy() { }
  virtual void DeviceAdded(const DBus::Path&) { }
  virtual void DeviceRemoved(const DBus::Path&) { }
};

class ModemSimpleProxy
    : public org::freedesktop::ModemManager::Modem::Simple_proxy,
      public DBus::ObjectProxy {
 public:
  ModemSimpleProxy(DBus::Connection& connection, const char* path, // NOLINT
                   const char* service) :
      DBus::ObjectProxy(connection, path, service) { }
  virtual ~ModemSimpleProxy() { }
};

class ModemProxy
    : public org::freedesktop::ModemManager::Modem_proxy,
      public DBus::ObjectProxy {
 public:
  ModemProxy(DBus::Connection& connection, const char* path, // NOLINT
             const char* service) :
      DBus::ObjectProxy(connection, path, service) { }
  virtual ~ModemProxy() { }
  virtual void StateChanged(const uint32_t&, const uint32_t&,
      const uint32_t&) { }
};

struct Modem {
  const char* service_;
  DBus::Path path_;

  Modem(const char* service, DBus::Path path)
      : service_(service), path_(path) { }

  Value* GetStatus(DBus::Connection& conn); // NOLINT
};

Value* FetchOneInterface(DBusPropertiesProxy& properties, // NOLINT
                         const char* interface,
                         DictionaryValue* result) {
  std::map<std::string, DBus::Variant> propsmap =
      properties.GetAll(interface);
  Value* propsdict = NULL;
  if (!chromeos::DBusPropertyMapToValue(propsmap, &propsdict))
    return NULL;
  std::string keypath = interface;
  ReplaceSubstringsAfterOffset(&keypath, 0, ".", "/");
  result->Set(keypath, propsdict);
  return propsdict;
}

Value* Modem::GetStatus(DBus::Connection& conn) { // NOLINT
  DictionaryValue* result = new DictionaryValue();
  result->SetString("service", service_);
  result->SetString("path", path_);

  ModemSimpleProxy simple = ModemSimpleProxy(conn, path_.c_str(), service_);
  Value* status = NULL;
  std::map<std::string, DBus::Variant> statusmap = simple.GetStatus();
  if (chromeos::DBusPropertyMapToValue(statusmap, &status))
    result->Set("status", status);

  ModemProxy modem = ModemProxy(conn, path_.c_str(), service_);
  DBus::Struct<std::string, std::string, std::string> infomap = modem.GetInfo();
  DictionaryValue* infodict = new DictionaryValue();
  infodict->SetString("manufacturer", infomap._1);
  infodict->SetString("modem", infomap._2);
  infodict->SetString("version", infomap._3);
  result->Set("info", infodict);

  DictionaryValue* props = new DictionaryValue();
  DBusPropertiesProxy properties = DBusPropertiesProxy(conn, path_.c_str(),
                                                       service_);
  FetchOneInterface(properties, kModemInterface, props);
  FetchOneInterface(properties, kModemSimpleInterface, props);
  uint32_t type = modem.Type();
  if (type == kModemTypeGsm) {
    FetchOneInterface(properties, kModemGsmInterface, props);
    FetchOneInterface(properties, kModemGsmCardInterface, props);
    FetchOneInterface(properties, kModemGsmNetworkInterface, props);
  } else {
    FetchOneInterface(properties, kModemCdmaInterface, props);
  }
  result->Set("properties", props);

  return result;
}

int main() {
  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  ModemManagerProxy modemmanager(conn, kMMPath, kMMService);
  ModemManagerProxy cromo(conn, kCromoPath, kCromoService);
  std::vector<Modem> modems;

  // These two try-catch blocks are to account for one of modemmanager or cromo
  // not being present. We don't want to crash if one of them isn't running, so
  // we swallow the DBus exception we get from the failed attempt to enumerate
  // devices.
  try {
    std::vector<DBus::Path> mm_modems = modemmanager.EnumerateDevices();
    for (size_t i = 0; i < mm_modems.size(); ++i)
      modems.push_back(Modem(kMMService, mm_modems[i]));
    // cpplint thinks this is a function call
  } catch(DBus::Error e) { }
  try {
    std::vector<DBus::Path> cromo_modems = cromo.EnumerateDevices();
    for (size_t i = 0; i < cromo_modems.size(); ++i)
      modems.push_back(Modem(kCromoService, cromo_modems[i]));
    // cpplint thinks this is a function call
  } catch(DBus::Error e) { }

  ListValue result;
  for (size_t i = 0; i < modems.size(); ++i)
    result.Append(modems[i].GetStatus(conn));
  std::string json;
  base::JSONWriter::Write(&result, true, &json);
  printf("%s\n", json.c_str());
  return 0;
}
