// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/shill_event.h"

#include "shill/wifi.h"

using std::string;

namespace shill {
const char WiFi::kSupplicantPath[]       = "/fi/w1/wpa_supplicant1";
const char WiFi::kSupplicantDBusAddr[]   = "fi.w1.wpa_supplicant1";
const char WiFi::kSupplicantWiFiDriver[] = "nl80211";
const char WiFi::kSupplicantErrorInterfaceExists[] =
    "fi.w1.wpa_supplicant1.InterfaceExists";

WiFi::SupplicantProcessProxy::SupplicantProcessProxy(DBus::Connection *bus)
    : DBus::ObjectProxy(*bus, kSupplicantPath, kSupplicantDBusAddr) {}

void WiFi::SupplicantProcessProxy::InterfaceAdded(
    const ::DBus::Path& path,
    const std::map<string, ::DBus::Variant> &properties) {
  LOG(INFO) << __func__;
  // XXX
}

void WiFi::SupplicantProcessProxy::InterfaceRemoved(const ::DBus::Path& path) {
  LOG(INFO) << __func__;
  // XXX
}

void WiFi::SupplicantProcessProxy::PropertiesChanged(
    const std::map<string, ::DBus::Variant>& properties) {
  LOG(INFO) << __func__;
  // XXX
}

WiFi::SupplicantInterfaceProxy::SupplicantInterfaceProxy(
    WiFi *wifi,
    DBus::Connection *bus,
    const ::DBus::Path &object_path)
    : wifi_(*wifi),
      DBus::ObjectProxy(*bus, object_path, kSupplicantDBusAddr) {}

void WiFi::SupplicantInterfaceProxy::ScanDone(const bool& success) {
  LOG(INFO) << __func__ << " " << success;
  if (success) {
    wifi_.ScanDone();
  }
}

void WiFi::SupplicantInterfaceProxy::BSSAdded(
    const ::DBus::Path &BSS,
    const std::map<string, ::DBus::Variant> &properties) {
  LOG(INFO) << __func__;
  wifi_.BSSAdded(BSS, properties);
}

void WiFi::SupplicantInterfaceProxy::BSSRemoved(const ::DBus::Path &BSS) {
  LOG(INFO) << __func__;
  // XXX
}

void WiFi::SupplicantInterfaceProxy::BlobAdded(const string &blobname) {
  LOG(INFO) << __func__;
  // XXX
}

void WiFi::SupplicantInterfaceProxy::BlobRemoved(const string &blobname) {
  LOG(INFO) << __func__;
  // XXX
}

void WiFi::SupplicantInterfaceProxy::NetworkAdded(
    const ::DBus::Path &network,
    const std::map<string, ::DBus::Variant> &properties) {
  LOG(INFO) << __func__;
  // XXX
}

void WiFi::SupplicantInterfaceProxy::NetworkRemoved(
    const ::DBus::Path &network) {
  LOG(INFO) << __func__;
  // XXX
}

void WiFi::SupplicantInterfaceProxy::NetworkSelected(
    const ::DBus::Path &network) {
  LOG(INFO) << __func__;
  // XXX
}

void WiFi::SupplicantInterfaceProxy::PropertiesChanged(
    const std::map<string, ::DBus::Variant> &properties) {
  LOG(INFO) << __func__;
  // XXX
}

// NB: we assume supplicant is already running. [quiche.20110518]
WiFi::WiFi(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Manager *manager,
           const std::string& link_name,
           int interface_index)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             interface_index),
      dbus_(DBus::Connection::SystemBus()), scan_pending_(false) {
  VLOG(2) << "WiFi device " << link_name_ << " initialized.";
}

WiFi::~WiFi() {
}

void WiFi::Start() {
  ::DBus::Path interface_path;

  supplicant_process_proxy_.reset(new SupplicantProcessProxy(&dbus_));
  try {
    std::map<string, DBus::Variant> create_interface_args;
    create_interface_args["Ifname"].writer().
        append_string(link_name_.c_str());
    create_interface_args["Driver"].writer().
        append_string(kSupplicantWiFiDriver);
    // TODO(quiche) create_interface_args["ConfigFile"].writer().append_string
    // (file with pkcs config info)
    interface_path =
        supplicant_process_proxy_->CreateInterface(create_interface_args);
  } catch (const DBus::Error e) {  // NOLINT
    if (!strcmp(e.name(), kSupplicantErrorInterfaceExists)) {
      interface_path =
          supplicant_process_proxy_->GetInterface(link_name_);
    } else {
      // XXX
    }
  }

  supplicant_interface_proxy_.reset(
      new SupplicantInterfaceProxy(this, &dbus_, interface_path));

  // TODO(quiche) set ApScan=1 and BSSExpireAge=190, like flimflam does?

  // clear out any networks that might previously have been configured
  // for this interface.
  supplicant_interface_proxy_->RemoveAllNetworks();

  // flush interface's BSS cache, so that we get BSSAdded signals for
  // all BSSes (not just new ones since the last scan).
  supplicant_interface_proxy_->FlushBSS(0);

  LOG(INFO) << "initiating Scan.";
  std::map<string, DBus::Variant> scan_args;
  scan_args["Type"].writer().append_string("active");
  // TODO(quiche) indicate scanning in UI
  supplicant_interface_proxy_->Scan(scan_args);
  scan_pending_ = true;
  Device::Start();
}

void WiFi::BSSAdded(
    const ::DBus::Path &BSS,
    const std::map<string, ::DBus::Variant> &properties) {
  std::vector<uint8_t> ssid = properties.find("SSID")->second;
  ssids_.push_back(string(ssid.begin(), ssid.end()));
}

void WiFi::ScanDone() {
  LOG(INFO) << __func__;

  scan_pending_ = false;
  for (std::vector<string>::iterator i(ssids_.begin());
       i != ssids_.end(); ++i) {
    LOG(INFO) << "found SSID " << *i;
  }
}

void WiFi::Stop() {
  LOG(INFO) << __func__;
  // XXX
  Device::Stop();
}

bool WiFi::TechnologyIs(const Device::Technology type) {
  return type == Device::kWifi;
}

}  // namespace shill
