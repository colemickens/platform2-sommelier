// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/debug_mode_tool.h"

#include <memory>

#include <base/memory/ptr_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <dbus/property.h>
#include <shill/dbus-proxies.h>

namespace debugd {

namespace {

const int kFlimflamLogLevelVerbose3 = -3;
const int kFlimflamLogLevelInfo = 0;

const char kSupplicantServiceName[] = "fi.w1.wpa_supplicant1";
const char kSupplicantObjectPath[] = "/f1/w1/wpa_supplicant1";
const char kSupplicantDebugLevel[] = "DebugLevel";

class SupplicantProxy {
 public:
  struct Properties : public dbus::PropertySet {
    dbus::Property<std::string> debug_level;

    explicit Properties(dbus::ObjectProxy* proxy)
        : dbus::PropertySet(proxy, kSupplicantServiceName,
                            dbus::PropertySet::PropertyChangedCallback()) {
      RegisterProperty(kSupplicantDebugLevel, &debug_level);
    }

    ~Properties() override = default;
  };

  explicit SupplicantProxy(scoped_refptr<dbus::Bus> bus)
      : bus_(bus),
        properties_(bus->GetObjectProxy(
            kSupplicantServiceName, dbus::ObjectPath(kSupplicantObjectPath))) {}

  ~SupplicantProxy() {}

  void SetDebugLevel(const std::string& level) {
    properties_.debug_level.SetAndBlock(level);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  Properties properties_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantProxy);
};

}  // namespace

#if USE_CELLULAR

namespace {

const char kDBusListNames[] = "ListNames";
const char kModemManager[] = "ModemManager";
const char kSetLogging[] = "SetLogging";

}  // namespace

#endif  // USE_CELLULAR

DebugModeTool::DebugModeTool(scoped_refptr<dbus::Bus> bus) : bus_(bus) {}

void DebugModeTool::SetDebugMode(const std::string& subsystem) {
  std::string flimflam_tags;
  std::string supplicant_level = "info";
  std::string modemmanager_level = "info";
  if (subsystem == "wifi") {
    flimflam_tags = "service+wifi+inet+device+manager";
    supplicant_level = "msgdump";
  } else if (subsystem == "wimax") {
    flimflam_tags = "service+wimax+device+manager";
  } else if (subsystem == "cellular") {
    flimflam_tags = "service+cellular+modem+device+manager";
    modemmanager_level = "debug";
  } else if (subsystem == "ethernet") {
    flimflam_tags = "service+ethernet+device+manager";
  } else if (subsystem == "none") {
    flimflam_tags = "";
  }

  auto shill = base::MakeUnique<org::chromium::flimflam::ManagerProxy>(bus_);
  if (shill) {
    shill->SetDebugTags(flimflam_tags, nullptr);
    if (flimflam_tags.length()) {
      shill->SetDebugLevel(kFlimflamLogLevelVerbose3, nullptr);
    } else {
      shill->SetDebugLevel(kFlimflamLogLevelInfo, nullptr);
    }
  }

  SupplicantProxy supplicant(bus_);
  supplicant.SetDebugLevel(supplicant_level);

  SetAllModemManagersLogging(modemmanager_level);
}

void DebugModeTool::GetAllModemManagers(std::vector<std::string>* managers) {
#if USE_CELLULAR
  managers->clear();

  dbus::ObjectProxy* proxy =
      bus_->GetObjectProxy(dbus::kDBusServiceName,
                           dbus::ObjectPath(dbus::kDBusServicePath));
  dbus::MethodCall method_call(dbus::kDBusInterface, kDBusListNames);
  std::unique_ptr<dbus::Response> response =
      proxy->CallMethodAndBlock(&method_call,
                                dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);

  std::vector<std::string> names;
  dbus::MessageReader reader(response.get());
  if (!reader.PopArrayOfStrings(&names))
    return;

  for (const auto& name : names) {
    if (name.find(kModemManager) != std::string::npos)
      managers->push_back(name);
  }
#endif  // USE_CELLULAR
}

void DebugModeTool::SetModemManagerLogging(const std::string& service_name,
                                           const std::string& service_path,
                                           const std::string& level) {
  dbus::ObjectProxy* proxy =
      bus_->GetObjectProxy(service_name, dbus::ObjectPath(service_path));
  dbus::MethodCall method_call(service_name, kSetLogging);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(level);
  proxy->CallMethodAndBlock(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
}

void DebugModeTool::SetAllModemManagersLogging(const std::string& level) {
#if USE_CELLULAR
  std::vector<std::string> managers;
  GetAllModemManagers(&managers);
  for (const auto& manager : managers) {
    if (manager == cromo::kCromoServiceName) {
      SetModemManagerLogging(cromo::kCromoServiceName,
                             cromo::kCromoServicePath,
                             (level == "err" ? "error" : level));
    } else if (manager == modemmanager::kModemManager1ServiceName) {
      SetModemManagerLogging(modemmanager::kModemManager1ServiceName,
                             modemmanager::kModemManager1ServicePath,
                             level);
    }
  }
#endif  // USE_CELLULAR
}

}  // namespace debugd
