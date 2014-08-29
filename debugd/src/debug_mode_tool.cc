// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/debug_mode_tool.h"

#include <base/file_util.h>
#include <chromeos/dbus/service_constants.h>

#include "dbus_proxies/org.freedesktop.DBus.Properties.h"

#if USE_CELLULAR
#include "dbus_proxies/org.freedesktop.ModemManager.h"
#include "dbus_proxies/org.freedesktop.ModemManager1.h"
#endif  // USE_CELLULAR

#include "shill/dbus_proxies/org.chromium.flimflam.Manager.h"

namespace debugd {

const char* const kSupplicantPath = "/fi/w1/wpa_supplicant1";
const char* const kSupplicantService = "fi.w1.wpa_supplicant1";
const char* const kSupplicantIface = "fi.w1.wpa_supplicant1";

class ManagerProxy : public org::chromium::flimflam::Manager_proxy,
                     public DBus::ObjectProxy {
 public:
  ManagerProxy(DBus::Connection* connection,
               const char* path,
               const char* service)
      : DBus::ObjectProxy(*connection, path, service) {}
  ~ManagerProxy() override = default;
  void PropertyChanged(const std::string&, const DBus::Variant&) override {}
  void StateChanged(const std::string&) override {}
};

#if USE_CELLULAR

const char* const kDBusListNames = "ListNames";
const char* const kModemManager = "ModemManager";

class ModemManagerProxy : public org::freedesktop::ModemManager_proxy,
                          public DBus::ObjectProxy {
 public:
  ModemManagerProxy(DBus::Connection* connection,
                    const char* path,
                    const char* service)
      : DBus::ObjectProxy(*connection, path, service) {}
  ~ModemManagerProxy() override = default;
  void DeviceAdded(const DBus::Path&) override {}
  void DeviceRemoved(const DBus::Path&) override {}
};

class ModemManager1Proxy : public org::freedesktop::ModemManager1_proxy,
                           public DBus::ObjectProxy {
 public:
  ModemManager1Proxy(DBus::Connection* connection,
                     const char* path,
                     const char* service)
      : DBus::ObjectProxy(*connection, path, service) {}
  ~ModemManager1Proxy() override = default;
};

#endif  // USE_CELLULAR

class PropertiesProxy : public org::freedesktop::DBus::Properties_proxy,
                        public DBus::ObjectProxy {
 public:
  PropertiesProxy(DBus::Connection* connection,
                  const char* path,
                  const char* service)
      : DBus::ObjectProxy(*connection, path, service) {}
  ~PropertiesProxy() override = default;
};

DebugModeTool::DebugModeTool(DBus::Connection* connection)
    : connection_(connection) {}

void DebugModeTool::SetDebugMode(const std::string& subsystem,
                                 DBus::Error*) {
  ManagerProxy flimflam(connection_,
                        shill::kFlimflamServicePath,
                        shill::kFlimflamServiceName);
  PropertiesProxy supplicant(connection_, kSupplicantPath, kSupplicantService);
  std::string flimflam_value;
  DBus::Variant supplicant_value;
  std::string modemmanager_value = "info";
  std::string supplicant_level = "info";
  if (subsystem == "wifi") {
    flimflam_value = "service+wifi+inet+device+manager";
    supplicant_level = "msgdump";
  } else if (subsystem == "wimax") {
    flimflam_value = "service+wimax+device+manager";
  } else if (subsystem == "cellular") {
    flimflam_value = "service+cellular+modem+device+manager";
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
#if USE_CELLULAR
  managers->clear();

  DBus::CallMessage msg;
  DBus::MessageIter iter;
  msg.destination(dbus::kDBusInterface);
  msg.interface(dbus::kDBusInterface);
  msg.member(kDBusListNames);
  msg.path(dbus::kDBusServicePath);
  DBus::Message ret = connection_->send_blocking(msg, -1);
  iter = ret.reader();
  iter = iter.recurse();
  while (!iter.at_end()) {
    std::string name(iter.get_string());
    if (name.find(kModemManager) != std::string::npos)
      managers->push_back(name);
    ++iter;
  }
#endif  // USE_CELLULAR
}

void DebugModeTool::SetAllModemManagersLogging(const std::string& level) {
#if USE_CELLULAR
  std::vector<std::string> managers;
  GetAllModemManagers(&managers);
  for (size_t i = 0; i < managers.size(); ++i) {
    const std::string& manager = managers[i];
    if (manager == cromo::kCromoServiceName) {
      ModemManagerProxy modemmanager(connection_,
                                     cromo::kCromoServicePath,
                                     cromo::kCromoServiceName);
      modemmanager.SetLogging(level == "err" ? "error" : level);
    } else if (manager == modemmanager::kModemManager1ServiceName) {
      ModemManager1Proxy modemmanager(connection_,
                                      modemmanager::kModemManager1ServicePath,
                                      modemmanager::kModemManager1ServiceName);
      modemmanager.SetLogging(level);
    }
  }
#endif  // USE_CELLULAR
}

}  // namespace debugd
