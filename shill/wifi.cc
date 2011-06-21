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
#include <base/string_number_conversions.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/shill_event.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_service.h"

#include "shill/wifi.h"

using std::string;

namespace shill {
const char WiFi::kSupplicantPath[]        = "/fi/w1/wpa_supplicant1";
const char WiFi::kSupplicantDBusAddr[]    = "fi.w1.wpa_supplicant1";
const char WiFi::kSupplicantWiFiDriver[]  = "nl80211";
const char WiFi::kSupplicantErrorInterfaceExists[] =
    "fi.w1.wpa_supplicant1.InterfaceExists";
const char WiFi::kSupplicantKeyModeNone[] = "NONE";

unsigned int WiFi::service_id_serial_ = 0;

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
           const string& link_name,
           int interface_index)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             interface_index),
      task_factory_(this),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      dbus_(DBus::Connection::SystemBus()),
      scan_pending_(false) {
  VLOG(2) << "WiFi device " << link_name << " initialized.";
}

WiFi::~WiFi() {
}

void WiFi::Start() {
  ::DBus::Path interface_path;

  supplicant_process_proxy_.reset(new SupplicantProcessProxy(&dbus_));
  try {
    std::map<string, DBus::Variant> create_interface_args;
    create_interface_args["Ifname"].writer().
        append_string(link_name().c_str());
    create_interface_args["Driver"].writer().
        append_string(kSupplicantWiFiDriver);
    // TODO(quiche) create_interface_args["ConfigFile"].writer().append_string
    // (file with pkcs config info)
    interface_path =
        supplicant_process_proxy_->CreateInterface(create_interface_args);
  } catch (const DBus::Error e) {  // NOLINT
    if (!strcmp(e.name(), kSupplicantErrorInterfaceExists)) {
      interface_path =
          supplicant_process_proxy_->GetInterface(link_name());
      // XXX crash here, if device missing?
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

void WiFi::Stop() {
  LOG(INFO) << __func__;
  // XXX
  Device::Stop();
}

bool WiFi::TechnologyIs(const Device::Technology type) {
  return type == Device::kWifi;
}

void WiFi::BSSAdded(
    const ::DBus::Path &BSS,
    const std::map<string, ::DBus::Variant> &properties) {
  // TODO(quiche): write test to verify correct behavior in the case
  // where we get multiple BSSAdded events for a single endpoint.
  // (old Endpoint's refcount should fall to zero, and old Endpoint
  // should be destroyed)
  //
  // note: we assume that BSSIDs are unique across endpoints. this
  // means that if an AP reuses the same BSSID for multiple SSIDs, we
  // lose.
  WiFiEndpointRefPtr endpoint(new WiFiEndpoint(properties));
  endpoint_by_bssid_[endpoint->bssid_hex()] = endpoint;
}

void WiFi::ScanDone() {
  LOG(INFO) << __func__;

  // defer handling of scan result processing, because that processing
  // may require the the registration of new D-Bus objects. and such
  // registration can't be done in the context of a D-Bus signal
  // handler.
  dispatcher_->PostTask(
      task_factory_.NewRunnableMethod(&WiFi::RealScanDone));
}

::DBus::Path WiFi::AddNetwork(
    const std::map<string, ::DBus::Variant> &args) {
  return supplicant_interface_proxy_->AddNetwork(args);
}

void WiFi::SelectNetwork(const ::DBus::Path &network) {
  supplicant_interface_proxy_->SelectNetwork(network);
}

void WiFi::RealScanDone() {
  LOG(INFO) << __func__;

  scan_pending_ = false;

  // TODO(quiche): group endpoints into services, instead of creating
  // a service for every endpoint.
  for (EndpointMap::iterator i(endpoint_by_bssid_.begin());
       i != endpoint_by_bssid_.end(); ++i) {
    const WiFiEndpoint &endpoint(*(i->second));
    string service_id_private;

    service_id_private =
        endpoint.ssid_hex() + "_" + endpoint.bssid_hex();
    if (service_by_private_id_.find(service_id_private) ==
        service_by_private_id_.end()) {
      unsigned int new_service_id_serial = service_id_serial_++;
      string service_name(base::UintToString(new_service_id_serial));

      LOG(INFO) << "found new endpoint. "
                << "ssid: " << endpoint.ssid_string() << ", "
                << "bssid: " << endpoint.bssid_string() << ", "
                << "signal: " << endpoint.signal_strength() << ", "
                << "service name: " << "/service/" << service_name;

      // XXX key mode should reflect endpoint params (not always use
      // kSupplicantKeyModeNone)
      ServiceRefPtr service(
          new WiFiService(control_interface_, dispatcher_, this,
                          endpoint.ssid(), endpoint.network_mode(),
                          kSupplicantKeyModeNone, service_name));
      services_.push_back(service);
      service_by_private_id_[service_id_private] = service;
    }
  }

  // TODO(quiche): register new services with manager
  // TODO(quiche): unregister removed services from manager
}

}  // namespace shill
