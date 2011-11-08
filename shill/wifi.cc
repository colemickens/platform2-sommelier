// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <netinet/ether.h>
#include <linux/if.h>  // Needs definitions from netinet/ether.h

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/device.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/key_value_store.h"
#include "shill/ieee80211.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/proxy_factory.h"
#include "shill/supplicant_interface_proxy_interface.h"
#include "shill/supplicant_process_proxy_interface.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_service.h"
#include "shill/wpa_supplicant.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

// statics
//
// Note that WiFi generates some manager-level errors, because it implements
// the Manager.GetWiFiService flimflam API. The API is implemented here,
// rather than in manager, to keep WiFi-specific logic in the right place.
const char WiFi::kManagerErrorPassphraseRequired[] = "must specify passphrase";
const char WiFi::kManagerErrorSSIDRequired[] = "must specify SSID";
const char WiFi::kManagerErrorSSIDTooLong[]  = "SSID is too long";
const char WiFi::kManagerErrorSSIDTooShort[] = "SSID is too short";
const char WiFi::kManagerErrorTypeRequired[] = "must specify service type";
const char WiFi::kManagerErrorUnsupportedSecurityMode[] =
    "security mode is unsupported";
const char WiFi::kManagerErrorUnsupportedServiceType[] =
    "service type is unsupported";
const char WiFi::kManagerErrorUnsupportedServiceMode[] =
    "service mode is unsupported";

// NB: we assume supplicant is already running. [quiche.20110518]
WiFi::WiFi(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Manager *manager,
           const string& link,
           const std::string &address,
           int interface_index)
    : Device(control_interface,
             dispatcher,
             manager,
             link,
             address,
             interface_index),
      proxy_factory_(ProxyFactory::GetInstance()),
      task_factory_(this),
      bgscan_short_interval_(0),
      bgscan_signal_threshold_(0),
      scan_pending_(false),
      scan_interval_(0),
      link_up_(false) {
  PropertyStore *store = this->mutable_store();
  store->RegisterString(flimflam::kBgscanMethodProperty, &bgscan_method_);
  store->RegisterUint16(flimflam::kBgscanShortIntervalProperty,
                 &bgscan_short_interval_);
  store->RegisterInt32(flimflam::kBgscanSignalThresholdProperty,
                &bgscan_signal_threshold_);

  // TODO(quiche): Decide if scan_pending_ is close enough to
  // "currently scanning" that we don't care, or if we want to track
  // scan pending/currently scanning/no scan scheduled as a tri-state
  // kind of thing.
  store->RegisterConstBool(flimflam::kScanningProperty, &scan_pending_);
  store->RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);
  VLOG(2) << "WiFi device " << link_name() << " initialized.";
}

WiFi::~WiFi() {}

void WiFi::Start() {
  ::DBus::Path interface_path;

  supplicant_process_proxy_.reset(
      proxy_factory_->CreateSupplicantProcessProxy(
          wpa_supplicant::kDBusPath, wpa_supplicant::kDBusAddr));
  try {
    std::map<string, DBus::Variant> create_interface_args;
    create_interface_args["Ifname"].writer().
        append_string(link_name().c_str());
    create_interface_args["Driver"].writer().
        append_string(wpa_supplicant::kDriverNL80211);
    // TODO(quiche) create_interface_args["ConfigFile"].writer().append_string
    // (file with pkcs config info)
    interface_path =
        supplicant_process_proxy_->CreateInterface(create_interface_args);
  } catch (const DBus::Error e) {  // NOLINT
    if (!strcmp(e.name(), wpa_supplicant::kErrorInterfaceExists)) {
      interface_path =
          supplicant_process_proxy_->GetInterface(link_name());
      // XXX crash here, if device missing?
    } else {
      // XXX
    }
  }

  supplicant_interface_proxy_.reset(
      proxy_factory_->CreateSupplicantInterfaceProxy(
          this, interface_path, wpa_supplicant::kDBusAddr));

  // TODO(quiche) set ApScan=1 and BSSExpireAge=190, like flimflam does?

  // clear out any networks that might previously have been configured
  // for this interface.
  supplicant_interface_proxy_->RemoveAllNetworks();

  // flush interface's BSS cache, so that we get BSSAdded signals for
  // all BSSes (not just new ones since the last scan).
  supplicant_interface_proxy_->FlushBSS(0);

  Scan(NULL);
  Device::Start();
}

void WiFi::Stop() {
  VLOG(2) << "WiFi " << link_name() << " stopping.";
  // TODO(quiche): remove interface from supplicant
  supplicant_interface_proxy_.reset();  // breaks a reference cycle
  supplicant_process_proxy_.reset();
  endpoint_by_bssid_.clear();
  service_by_private_id_.clear();       // breaks reference cycles

  for (vector<WiFiServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    manager()->DeregisterService(*it);
  }
  services_.clear();                  // breaks reference cycles

  Device::Stop();
  // XXX anything else to do?

  VLOG(3) << "WiFi " << link_name() << " task_factory_ "
          << (task_factory_.empty() ? "is empty." : "is not empty.");
  VLOG(3) << "WiFi " << link_name() << " supplicant_process_proxy_ "
          << (supplicant_process_proxy_.get() ? "is set." : "is not set.");
  VLOG(3) << "WiFi " << link_name() << " supplicant_interface_proxy_ "
          << (supplicant_interface_proxy_.get() ? "is set." : "is not set.");
  VLOG(3) << "WiFi " << link_name() << " has " << endpoint_by_bssid_.size()
          << " EndpointMap entries.";
  VLOG(3) << "WiFi " << link_name() << " has " << service_by_private_id_.size()
          << " ServiceMap entries.";
}

void WiFi::Scan(Error */*error*/) {
  LOG(INFO) << __func__;

  // needs to send a D-Bus message, but may be called from D-Bus
  // signal handler context (via Manager::RequestScan). so defer work
  // to event loop.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFi::ScanTask));
}

bool WiFi::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWifi;
}

void WiFi::LinkEvent(unsigned int flags, unsigned int change) {
  // TODO(quiche): figure out how to relate these events to supplicant
  // events. e.g., may be we can ignore LinkEvent, in favor of events
  // from SupplicantInterfaceProxy?
  Device::LinkEvent(flags, change);
  if ((flags & IFF_LOWER_UP) != 0 && !link_up_) {
    LOG(INFO) << link_name() << " is up; should start L3!";
    link_up_ = true;
    if (AcquireDHCPConfig()) {
      SetServiceState(Service::kStateConfiguring);
    } else {
      LOG(ERROR) << "Unable to acquire DHCP config.";
    }
  } else if ((flags & IFF_LOWER_UP) == 0 && link_up_) {
    LOG(INFO) << link_name() << " is down";
    link_up_ = false;
    // TODO(quiche): attempt to reconnect to current SSID, another SSID,
    // or initiate a scan.
  }
}

void WiFi::BSSAdded(
    const ::DBus::Path &/*BSS*/,
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
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFi::ScanDoneTask));
}

void WiFi::ConnectTo(WiFiService *service,
                     const map<string, DBus::Variant> &service_params) {
  DBus::Path network_path;

  // TODO(quiche): handle cases where already connected

  // TODO(quiche): set scan_ssid=1 in service_params, like flimflam does?
  try {
    network_path =
        supplicant_interface_proxy_->AddNetwork(service_params);
  } catch (const DBus::Error e) {  // NOLINT
    LOG(ERROR) << "exception while adding network: " << e.what();
    return;
  }

  supplicant_interface_proxy_->SelectNetwork(network_path);
  // XXX add to favorite networks list?

  // SelectService here (instead of in LinkEvent, like Ethernet), so
  // that, if we fail to bring up L2, we can attribute failure correctly.
  //
  // TODO(quiche): when we add code for dealing with connection failures,
  // reconsider if this is the right place to change the selected service.
  // see discussion in crosbug.com/20191.
  SelectService(service);
}

ByteArrays WiFi::GetHiddenSSIDList() {
  // Create a unique set of hidden SSIDs.
  set<ByteArray> hidden_ssids_set;
  for (vector<WiFiServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    if ((*it)->hidden_ssid()) {
      hidden_ssids_set.insert((*it)->ssid());
    }
  }
  ByteArrays hidden_ssids(hidden_ssids_set.begin(), hidden_ssids_set.end());
  if (!hidden_ssids.empty()) {
    // TODO(pstew): Devise a better method for time-sharing with SSIDs that do
    // not fit in.
    if (hidden_ssids.size() >= wpa_supplicant::kScanMaxSSIDsPerScan) {
      hidden_ssids.erase(
          hidden_ssids.begin() + wpa_supplicant::kScanMaxSSIDsPerScan - 1,
          hidden_ssids.end());
    }
    // Add Broadcast SSID, signified by an empty ByteArray.  If we specify
    // SSIDs to wpa_supplicant, we need to explicitly specify the default
    // behavior of doing a broadcast probe.
    hidden_ssids.push_back(ByteArray());
  }
  return hidden_ssids;
}

void WiFi::ScanDoneTask() {
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
      LOG(INFO) << "found new endpoint. "
                << "ssid: " << endpoint.ssid_string() << ", "
                << "bssid: " << endpoint.bssid_string() << ", "
                << "signal: " << endpoint.signal_strength() << ", "
                << "security: " << endpoint.security_mode();

      const bool hidden_ssid = false;
      WiFiServiceRefPtr service(
          new WiFiService(control_interface(),
                          dispatcher(),
                          manager(),
                          this,
                          endpoint.ssid(),
                          endpoint.network_mode(),
                          endpoint.security_mode(),
                          hidden_ssid));
      services_.push_back(service);
      service_by_private_id_[service_id_private] = service;
      manager()->RegisterService(service);

      LOG(INFO) << "new service " << service->GetRpcIdentifier();
    }
  }

  // TODO(quiche): unregister removed services from manager
}

void WiFi::ScanTask() {
  VLOG(2) << "WiFi " << link_name() << " scan requested.";
  std::map<string, DBus::Variant> scan_args;
  scan_args[wpa_supplicant::kPropertyScanType].writer().
      append_string(wpa_supplicant::kScanTypeActive);

  ByteArrays hidden_ssids = GetHiddenSSIDList();
  if (!hidden_ssids.empty()) {
    scan_args[wpa_supplicant::kPropertyScanSSIDs] =
        DBusAdaptor::ByteArraysToVariant(hidden_ssids);
  }

  // TODO(quiche): Indicate scanning in UI. crosbug.com/14887
  supplicant_interface_proxy_->Scan(scan_args);
  scan_pending_ = true;
}

// used by manager, via static WiFi::GetService method
WiFiServiceRefPtr WiFi::GetService(const KeyValueStore &args, Error *error) {
  if (!args.ContainsString(flimflam::kTypeProperty)) {
    error->Populate(Error::kInvalidArguments, kManagerErrorTypeRequired);
    return NULL;
  }

  if (args.GetString(flimflam::kTypeProperty) != flimflam::kTypeWifi) {
    error->Populate(Error::kNotSupported, kManagerErrorUnsupportedServiceType);
    return NULL;
  }

  if (args.ContainsString(flimflam::kModeProperty) &&
      args.GetString(flimflam::kModeProperty) !=
      flimflam::kModeManaged) {
    error->Populate(Error::kNotSupported, kManagerErrorUnsupportedServiceMode);
    return NULL;
  }

  if (!args.ContainsString(flimflam::kSSIDProperty)) {
    error->Populate(Error::kInvalidArguments, kManagerErrorSSIDRequired);
    return NULL;
  }

  string ssid = args.GetString(flimflam::kSSIDProperty);
  if (ssid.length() < 1) {
    error->Populate(Error::kInvalidNetworkName, kManagerErrorSSIDTooShort);
    return NULL;
  }

  if (ssid.length() > IEEE_80211::kMaxSSIDLen) {
    error->Populate(Error::kInvalidNetworkName, kManagerErrorSSIDTooLong);
    return NULL;
  }

  string security_method;
  if (args.ContainsString(flimflam::kSecurityProperty)) {
    security_method = args.GetString(flimflam::kSecurityProperty);
  } else {
    security_method = flimflam::kSecurityNone;
  }

  if (security_method != flimflam::kSecurityNone &&
      security_method != flimflam::kSecurityWep &&
      security_method != flimflam::kSecurityPsk &&
      security_method != flimflam::kSecurityWpa &&
      security_method != flimflam::kSecurityRsn &&
      security_method != flimflam::kSecurity8021x) {
    error->Populate(Error::kNotSupported,
                    kManagerErrorUnsupportedSecurityMode);
    return NULL;
  }

  if ((security_method == flimflam::kSecurityWep ||
       security_method == flimflam::kSecurityPsk ||
       security_method == flimflam::kSecurityWpa ||
       security_method == flimflam::kSecurityRsn) &&
      !args.ContainsString(flimflam::kPassphraseProperty)) {
    error->Populate(Error::kInvalidArguments,
                    kManagerErrorPassphraseRequired);
    return NULL;
  }

  bool hidden_ssid = false;
  if (args.ContainsBool(flimflam::kWifiHiddenSsid)) {
    hidden_ssid = args.GetBool(flimflam::kWifiHiddenSsid);
  }

  WiFiService *service = NULL;

  // TODO(quiche): search for existing service

  if (service == NULL) {
    service = new WiFiService(control_interface(),
                              dispatcher(),
                              manager(),
                              this,
                              vector<uint8_t>(ssid.begin(), ssid.end()),
                              flimflam::kModeManaged,
                              security_method,
                              hidden_ssid);
    services_.push_back(service);
    // TODO(quiche): add to service_by_private_id_?
    // TODO(quiche): register service with manager
  }

  if (security_method == flimflam::kSecurityWep ||
      security_method == flimflam::kSecurityPsk ||
      security_method == flimflam::kSecurityWpa ||
      security_method == flimflam::kSecurityRsn) {
    service->SetPassphrase(args.GetString(flimflam::kPassphraseProperty),
                           error);
    if (error->IsFailure()) {
      return NULL;
    }
  }

  // TODO(quiche): apply any other configuration parameters

  return service;
}

}  // namespace shill
