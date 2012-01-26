// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus-c++/dbus.h>
#include <stdio.h>

#include <base/json/json_writer.h>
#include <base/string_util.h>
#include <base/values.h>
#include <chromeos/utility.h>

#include "proxies/org.chromium.flimflam.Device.h"
#include "proxies/org.chromium.flimflam.IPConfig.h"
#include "proxies/org.chromium.flimflam.Manager.h"
#include "proxies/org.chromium.flimflam.Service.h"

const char* kFlimflamPath = "/";
const char* kFlimflamService = "org.chromium.flimflam";

class DeviceProxy
    : public org::chromium::flimflam::Device_proxy,
      public DBus::ObjectProxy {
  public:
    DeviceProxy(DBus::Connection& connection, const char* path, // NOLINT
                const char* service) :
        DBus::ObjectProxy(connection, path, service) { }
    virtual ~DeviceProxy() { }
    virtual void PropertyChanged(const std::string&, const DBus::Variant&) { }
};

class IPConfigProxy
    : public org::chromium::flimflam::IPConfig_proxy,
      public DBus::ObjectProxy {
  public:
    IPConfigProxy(DBus::Connection& connection, const char* path, // NOLINT
                const char* service) :
        DBus::ObjectProxy(connection, path, service) { }
    virtual ~IPConfigProxy() { }
    virtual void PropertyChanged(const std::string&, const DBus::Variant&) { }
};

class ManagerProxy
    : public org::chromium::flimflam::Manager_proxy,
      public DBus::ObjectProxy {
  public:
    ManagerProxy(DBus::Connection& connection, const char* path, // NOLINT
                 const char* service) :
        DBus::ObjectProxy(connection, path, service) { }
    virtual ~ManagerProxy() { }
    virtual void PropertyChanged(const std::string&, const DBus::Variant&) { }
    virtual void StateChanged(const std::string&) { }
};

class ServiceProxy
    : public org::chromium::flimflam::Service_proxy,
      public DBus::ObjectProxy {
  public:
    ServiceProxy(DBus::Connection& connection, const char* path, // NOLINT
                 const char* service) :
        DBus::ObjectProxy(connection, path, service) { }
    virtual ~ServiceProxy() { }
    virtual void PropertyChanged(const std::string&, const DBus::Variant&) { }
};

Value* GetService(DBus::Connection& conn, DBus::Path& path) { // NOLINT
  ServiceProxy service = ServiceProxy(conn, path.c_str(), kFlimflamService);
  std::map<std::string, DBus::Variant> props = service.GetProperties();
  Value* v = NULL;
  chromeos::DBusPropertyMapToValue(props, &v);
  return v;
}

Value* GetServices(DBus::Connection& conn, ManagerProxy& flimflam) { // NOLINT
  std::map<std::string, DBus::Variant> props = flimflam.GetProperties();
  DictionaryValue* dv = new DictionaryValue();
  DBus::Variant& devices = props["Services"];
  std::vector<DBus::Path> paths = devices;
  for (std::vector<DBus::Path>::iterator it = paths.begin();
       it != paths.end();
       ++it) {
    Value* v = GetService(conn, *it);
    if (v)
      dv->Set(*it, v);
  }
  return dv;
}

Value* GetIPConfig(DBus::Connection& conn, DBus::Path& path) { // NOLINT
  IPConfigProxy ipconfig = IPConfigProxy(conn, path.c_str(), kFlimflamService);
  std::map<std::string, DBus::Variant> props = ipconfig.GetProperties();
  Value* v = NULL;
  chromeos::DBusPropertyMapToValue(props, &v);
  return v;
}

Value* GetDevice(DBus::Connection& conn, DBus::Path& path) { // NOLINT
  DeviceProxy device = DeviceProxy(conn, path.c_str(), kFlimflamService);
  std::map<std::string, DBus::Variant> props = device.GetProperties();
  DictionaryValue* ipconfigs = NULL;
  if (props.count("IPConfigs") == 1) {
    ipconfigs = new DictionaryValue();
    // Turn IPConfigs into real objects.
    DBus::Variant& ipconfig_paths = props["IPConfigs"];
    std::vector<DBus::Path> paths = ipconfig_paths;
    for (std::vector<DBus::Path>::iterator it = paths.begin();
         it != paths.end();
         ++it) {
      Value* v = GetIPConfig(conn, *it);
      if (v)
        ipconfigs->Set(*it, v);
    }
    props.erase("IPConfigs");
  }
  Value* v = NULL;
  chromeos::DBusPropertyMapToValue(props, &v);
  DictionaryValue* dv = reinterpret_cast<DictionaryValue*>(v);
  if (ipconfigs)
    dv->Set("ipconfigs", ipconfigs);
  return v;
}

Value* GetDevices(DBus::Connection& conn, ManagerProxy& flimflam) { // NOLINT
  std::map<std::string, DBus::Variant> props = flimflam.GetProperties();
  DictionaryValue* dv = new DictionaryValue();
  DBus::Variant& devices = props["Devices"];
  std::vector<DBus::Path> paths = devices;
  for (std::vector<DBus::Path>::iterator it = paths.begin();
       it != paths.end();
       ++it) {
    Value* v = GetDevice(conn, *it);
    if (v)
      dv->Set(*it, v);
  }
  return dv;
}

int main() {
  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  ManagerProxy manager(conn, kFlimflamPath, kFlimflamService);
  DictionaryValue result;

  Value* devices = GetDevices(conn, manager);
  result.Set("devices", devices);

  Value* services = GetServices(conn, manager);
  result.Set("services", services);

  std::string json;
  base::JSONWriter::Write(&result, true, &json);
  printf("%s\n", json.c_str());
  return 0;
}
