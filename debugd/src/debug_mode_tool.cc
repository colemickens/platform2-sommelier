// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug_mode_tool.h"

#include <base/file_util.h>

#include "proxies/org.chromium.flimflam.Manager.h"
#include "proxies/org.freedesktop.DBus.Properties.h"
#include "proxies/org.freedesktop.ModemManager.h"

namespace debugd {

const char* const kFlimflamPath = "/";
const char* const kFlimflamService = "org.chromium.flimflam";

const char* const kSupplicantPath = "/fi/w1/wpa_supplicant1";
const char* const kSupplicantService = "fi.w1.wpa_supplicant1";
const char* const kSupplicantIface = "fi.w1.wpa_supplicant1";

const char* const kModemManagerPath = "/org/freedesktop/ModemManager";
const char* const kModemManagerService = "org.freedesktop.ModemManager";

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
  ModemManagerProxy modemmanager(connection_, kModemManagerPath,
                                 kModemManagerService);
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
  modemmanager.SetLogging(modemmanager_value);
}

};  // namespace debugd
