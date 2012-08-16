// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug_mode_tool.h"

#include <base/file_util.h>

#include "proxies/org.chromium.flimflam.Manager.h"
#include "proxies/org.freedesktop.DBus.Properties.h"
#include "proxies/org.freedesktop.ModemManager.h"
#include "proxies/org.freedesktop.ModemManager1.h"

namespace debugd {

const char* const kDBusPath = "/org/freedesktop/DBus";
const char* const kDBusInterface = "org.freedesktop.DBus";
const char* const kDBusListNames = "ListNames";

const char* const kFlimflamPath = "/";
const char* const kFlimflamService = "org.chromium.flimflam";

const char* const kSupplicantPath = "/fi/w1/wpa_supplicant1";
const char* const kSupplicantService = "fi.w1.wpa_supplicant1";
const char* const kSupplicantIface = "fi.w1.wpa_supplicant1";

const char* const kModemManager = "ModemManager";

const char* const kShillModemManagerPath = "/org/chromium/ModemManager";
const char* const kShillModemManagerService = "org.chromium.ModemManager";

const char* const kModemManagerPath = "/org/freedesktop/ModemManager";
const char* const kModemManagerService = "org.freedesktop.ModemManager";

const char* const kModemManager1Path = "/org/freedesktop/ModemManager1";
const char* const kModemManager1Service = "org.freedesktop.ModemManager1";

class ManagerProxy
    : public org::chromium::flimflam::Manager_proxy,
      public DBus::ObjectProxy {
  public:
    ManagerProxy(DBus::Connection* connection, const char* path,
                 const char* service) :
        DBus::ObjectProxy(*connection, path, service) { }
    virtual ~ManagerProxy() { }
    virtual void PropertyChanged(const std::string&, const DBus::Variant&) { }
    virtual void StateChanged(const std::string&) { }
};

class ModemManagerProxy
    : public org::freedesktop::ModemManager_proxy,
      public DBus::ObjectProxy {
  public:
    ModemManagerProxy(DBus::Connection* connection, const char* path,
                      const char* service) :
        DBus::ObjectProxy(*connection, path, service) { }
    virtual ~ModemManagerProxy() { }
    virtual void DeviceAdded(const DBus::Path&) { }
    virtual void DeviceRemoved(const DBus::Path&) { }
};

class ModemManager1Proxy
    : public org::freedesktop::ModemManager1_proxy,
      public DBus::ObjectProxy {
  public:
    ModemManager1Proxy(DBus::Connection* connection, const char* path,
                       const char* service) :
        DBus::ObjectProxy(*connection, path, service) { }
    virtual ~ModemManager1Proxy() { }
    virtual void DeviceAdded(const DBus::Path&) { }
    virtual void DeviceRemoved(const DBus::Path&) { }
};

class PropertiesProxy
    : public org::freedesktop::DBus::Properties_proxy,
      public DBus::ObjectProxy {
  public:
    PropertiesProxy(DBus::Connection* connection, const char* path,
                    const char* service) :
        DBus::ObjectProxy(*connection, path, service) { }
    virtual ~PropertiesProxy() { }
};

DebugModeTool::DebugModeTool(DBus::Connection* connection)
    : connection_(connection) { }

DebugModeTool::~DebugModeTool() { }

void DebugModeTool::SetDebugMode(const std::string& subsystem,
                                 DBus::Error*) {
  ManagerProxy flimflam(connection_, kFlimflamPath, kFlimflamService);
  PropertiesProxy supplicant(connection_, kSupplicantPath, kSupplicantService);
  std::string flimflam_value;
  DBus::Variant supplicant_value;
  std::string modemmanager_value = "info";
  std::string supplicant_level = "info";
  if (subsystem == "wifi") {
    flimflam_value = "service+wifi+inet+device+manager";
    supplicant_level = "msgdump";
  } else if (subsystem == "cellular") {
    flimflam_value = "service+modem+device+manager";
  } else if (subsystem == "ethernet") {
    flimflam_value = "service+ethernet+device+manager";
  } else if (subsystem == "none") {
    flimflam_value = "";
  }
  flimflam.SetDebugTags(flimflam_value);
  supplicant_value.writer().append_string(supplicant_level.c_str());
  supplicant.Set(kSupplicantIface, "DebugLevel", supplicant_value);
  SetAllModemManagersLogging(modemmanager_value);
}

void DebugModeTool::GetAllModemManagers(std::vector<std::string>* managers) {
  managers->clear();

  DBus::CallMessage msg;
  DBus::MessageIter iter;
  msg.destination(kDBusInterface);
  msg.interface(kDBusInterface);
  msg.member(kDBusListNames);
  msg.path(kDBusPath);
  DBus::Message ret = connection_->send_blocking(msg, -1);
  iter = ret.reader();
  iter = iter.recurse();
  while (!iter.at_end()) {
    std::string name(iter.get_string());
    if (name.find(kModemManager) != std::string::npos)
      managers->push_back(name);
    ++iter;
  }
}

void DebugModeTool::SetAllModemManagersLogging(const std::string& level) {
  std::vector<std::string> managers;
  GetAllModemManagers(&managers);
  for (size_t i = 0; i < managers.size(); ++i) {
    const std::string& manager = managers[i];
    if (manager == kShillModemManagerService) {
      ModemManagerProxy modemmanager(connection_, kShillModemManagerPath,
                                     kShillModemManagerService);
      modemmanager.SetLogging(level == "err" ? "error" : level);
    } else if (manager == kModemManagerService) {
      ModemManagerProxy modemmanager(connection_, kModemManagerPath,
                                     kModemManagerService);
      modemmanager.SetLogging(level);
    } else if (manager == kModemManager1Service) {
      ModemManager1Proxy modemmanager(connection_, kModemManager1Path,
                                      kModemManager1Service);
      modemmanager.SetLogging(level);
    }
  }
}

};  // namespace debugd
