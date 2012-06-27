// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_endpoint.h"

#include <base/logging.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/ieee80211.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/supplicant_bss_proxy_interface.h"
#include "shill/wifi.h"
#include "shill/wifi_endpoint.h"
#include "shill/wpa_supplicant.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

WiFiEndpoint::WiFiEndpoint(ProxyFactory *proxy_factory,
                           const WiFiRefPtr &device,
                           const string &rpc_id,
                           const map<string, ::DBus::Variant> &properties)
    : frequency_(0),
      proxy_factory_(proxy_factory),
      device_(device),
      rpc_id_(rpc_id) {
  // XXX will segfault on missing properties
  ssid_ =
      properties.find(wpa_supplicant::kBSSPropertySSID)->second.
      operator std::vector<uint8_t>();
  bssid_ =
      properties.find(wpa_supplicant::kBSSPropertyBSSID)->second.
      operator std::vector<uint8_t>();
  signal_strength_ =
      properties.find(wpa_supplicant::kBSSPropertySignal)->second.
      reader().get_int16();
  map<string, ::DBus::Variant>::const_iterator it =
      properties.find(wpa_supplicant::kBSSPropertyFrequency);
  if (it != properties.end())
    frequency_ = it->second.reader().get_uint16();
  physical_mode_ = DeterminePhyMode(properties, frequency_);
  network_mode_ = ParseMode(
      properties.find(wpa_supplicant::kBSSPropertyMode)->second);
  security_mode_ = ParseSecurity(properties);

  if (network_mode_.empty()) {
    // XXX log error?
  }

  ssid_string_ = string(ssid_.begin(), ssid_.end());
  WiFi::SanitizeSSID(&ssid_string_);
  ssid_hex_ = base::HexEncode(&(*ssid_.begin()), ssid_.size());
  bssid_string_ = StringPrintf("%02x:%02x:%02x:%02x:%02x:%02x",
                               bssid_[0], bssid_[1], bssid_[2],
                               bssid_[3], bssid_[4], bssid_[5]);
  bssid_hex_ = base::HexEncode(&(*bssid_.begin()), bssid_.size());
}

WiFiEndpoint::~WiFiEndpoint() {}

void WiFiEndpoint::Start() {
  supplicant_bss_proxy_.reset(
      proxy_factory_->CreateSupplicantBSSProxy(
          this, rpc_id_, wpa_supplicant::kDBusAddr));
}

void WiFiEndpoint::PropertiesChanged(
    const map<string, ::DBus::Variant> &properties) {
  SLOG(WiFi, 2) << __func__;
  map<string, ::DBus::Variant>::const_iterator properties_it =
      properties.find(wpa_supplicant::kBSSPropertySignal);
  if (properties_it != properties.end()) {
    signal_strength_ = properties_it->second.reader().get_int16();
    SLOG(WiFi, 2) << "WiFiEndpoint " << bssid_string_ << " signal is now "
                  << signal_strength_;
    device_->NotifyEndpointChanged(*this);
  }
}

// static
uint32_t WiFiEndpoint::ModeStringToUint(const std::string &mode_string) {
  if (mode_string == flimflam::kModeManaged)
    return wpa_supplicant::kNetworkModeInfrastructureInt;
  else if (mode_string == flimflam::kModeAdhoc)
    return wpa_supplicant::kNetworkModeAdHocInt;
  else
    NOTIMPLEMENTED() << "Shill dos not support " << mode_string
                     << " mode at this time.";
  return 0;
}

const vector<uint8_t> &WiFiEndpoint::ssid() const {
  return ssid_;
}

const string &WiFiEndpoint::ssid_string() const {
  return ssid_string_;
}

const string &WiFiEndpoint::ssid_hex() const {
  return ssid_hex_;
}

const string &WiFiEndpoint::bssid_string() const {
  return bssid_string_;
}

const string &WiFiEndpoint::bssid_hex() const {
  return bssid_hex_;
}

int16_t WiFiEndpoint::signal_strength() const {
  return signal_strength_;
}

uint16 WiFiEndpoint::frequency() const {
  return frequency_;
}

uint16 WiFiEndpoint::physical_mode() const {
  return physical_mode_;
}

const string &WiFiEndpoint::network_mode() const {
  return network_mode_;
}

const string &WiFiEndpoint::security_mode() const {
  return security_mode_;
}

// static
WiFiEndpoint *WiFiEndpoint::MakeOpenEndpoint(ProxyFactory *proxy_factory,
                                             const WiFiRefPtr &wifi,
                                             const string &ssid,
                                             const string &bssid,
                                             uint16 frequency,
                                             int16 signal_dbm) {
  map <string, ::DBus::Variant> args;
  ::DBus::MessageIter writer;

  writer = args[wpa_supplicant::kBSSPropertySSID].writer();
  writer << vector<uint8_t>(ssid.begin(), ssid.end());

  string bssid_nosep;
  RemoveChars(bssid, ":", &bssid_nosep);
  vector<uint8_t> bssid_bytes;
  base::HexStringToBytes(bssid_nosep, &bssid_bytes);
  writer = args[wpa_supplicant::kBSSPropertyBSSID].writer();
  writer << bssid_bytes;

  args[wpa_supplicant::kBSSPropertySignal].writer().append_int16(signal_dbm);
  args[wpa_supplicant::kBSSPropertyFrequency].writer().append_uint16(frequency);
  args[wpa_supplicant::kBSSPropertyMode].writer().append_string(
      wpa_supplicant::kNetworkModeInfrastructure);
  // We indicate this is an open BSS by leaving out all security properties.

  return new WiFiEndpoint(
      proxy_factory, wifi, bssid, args); // |bssid| fakes an RPC ID
}

// static
const char *WiFiEndpoint::ParseMode(const string &mode_string) {
  if (mode_string == wpa_supplicant::kNetworkModeInfrastructure) {
    return flimflam::kModeManaged;
  } else if (mode_string == wpa_supplicant::kNetworkModeAdHoc) {
    return flimflam::kModeAdhoc;
  } else if (mode_string == wpa_supplicant::kNetworkModeAccessPoint) {
    NOTREACHED() << "Shill does not support AP mode at this time.";
    return NULL;
  } else {
    NOTREACHED() << "Unknown WiFi endpoint mode!";
    return NULL;
  }
}

// static
const char *WiFiEndpoint::ParseSecurity(
    const map<string, ::DBus::Variant> &properties) {
  set<KeyManagement> rsn_key_management_methods;
  if (ContainsKey(properties, wpa_supplicant::kPropertyRSN)) {
    // TODO(quiche): check type before casting
    const map<string, ::DBus::Variant> rsn_properties(
        properties.find(wpa_supplicant::kPropertyRSN)->second.
        operator map<string, ::DBus::Variant>());
    ParseKeyManagementMethods(rsn_properties, &rsn_key_management_methods);
  }

  set<KeyManagement> wpa_key_management_methods;
  if (ContainsKey(properties, wpa_supplicant::kPropertyWPA)) {
    // TODO(quiche): check type before casting
    const map<string, ::DBus::Variant> rsn_properties(
        properties.find(wpa_supplicant::kPropertyWPA)->second.
        operator map<string, ::DBus::Variant>());
    ParseKeyManagementMethods(rsn_properties, &wpa_key_management_methods);
  }

  bool wep_privacy = false;
  if (ContainsKey(properties, wpa_supplicant::kPropertyPrivacy)) {
    wep_privacy = properties.find(wpa_supplicant::kPropertyPrivacy)->second.
        reader().get_bool();
  }

  if (ContainsKey(rsn_key_management_methods, kKeyManagement802_1x) ||
      ContainsKey(wpa_key_management_methods, kKeyManagement802_1x)) {
    return flimflam::kSecurity8021x;
  } else if (ContainsKey(rsn_key_management_methods, kKeyManagementPSK)) {
    return flimflam::kSecurityRsn;
  } else if (ContainsKey(wpa_key_management_methods, kKeyManagementPSK)) {
    return flimflam::kSecurityWpa;
  } else if (wep_privacy) {
    return flimflam::kSecurityWep;
  } else {
    return flimflam::kSecurityNone;
  }
}

// static
void WiFiEndpoint::ParseKeyManagementMethods(
    const map<string, ::DBus::Variant> &security_method_properties,
    set<KeyManagement> *key_management_methods) {
  if (!ContainsKey(security_method_properties,
                   wpa_supplicant::kSecurityMethodPropertyKeyManagement)) {
    return;
  }

  // TODO(quiche): check type before cast
  const vector<string> key_management_vec =
      security_method_properties.
      find(wpa_supplicant::kSecurityMethodPropertyKeyManagement)->second.
      operator vector<string>();
  for (vector<string>::const_iterator it = key_management_vec.begin();
       it != key_management_vec.end();
       ++it) {
    if (EndsWith(*it, wpa_supplicant::kKeyManagementMethodSuffixEAP, true)) {
      key_management_methods->insert(kKeyManagement802_1x);
    } else if (
        EndsWith(*it, wpa_supplicant::kKeyManagementMethodSuffixPSK, true)) {
      key_management_methods->insert(kKeyManagementPSK);
    }
  }
}

// static
Metrics::WiFiNetworkPhyMode WiFiEndpoint::DeterminePhyMode(
    const map<string, ::DBus::Variant> &properties, uint16 frequency) {
  uint32_t max_rate = 0;
  map<string, ::DBus::Variant>::const_iterator it =
      properties.find(wpa_supplicant::kBSSPropertyRates);
  if (it != properties.end()) {
    vector<uint32_t> rates = it->second.operator vector<uint32_t>();
    if (rates.size() > 0)
      max_rate = rates[0];  // Rates are sorted in descending order
  }

  Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
  it = properties.find(wpa_supplicant::kBSSPropertyIEs);
  if (it != properties.end()) {
    phy_mode = ParseIEsForPhyMode(it->second.operator vector<uint8_t>());
    if (phy_mode != Metrics::kWiFiNetworkPhyModeUndef)
      return phy_mode;
  }

  if (frequency < 3000) {
    // 2.4GHz legacy, check for tx rate for 11b-only
    // (note 22M is valid)
    if (max_rate < 24000000)
      phy_mode = Metrics::kWiFiNetworkPhyMode11b;
    else
      phy_mode = Metrics::kWiFiNetworkPhyMode11g;
  } else {
    phy_mode = Metrics::kWiFiNetworkPhyMode11a;
  }

  return phy_mode;
}

// static
Metrics::WiFiNetworkPhyMode WiFiEndpoint::ParseIEsForPhyMode(
    const vector<uint8_t> &ies) {
  // Format of an information element:
  //    1       1           3           1 - 252
  // +------+--------+------------+----------------+
  // | Type | Length | OUI        | Data           |
  // +------+--------+------------+----------------+
  Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
  vector<uint8_t>::const_iterator it;
  for (it = ies.begin();
       it + 1 < ies.end();  // +1 to ensure Length field is in valid memory
       it += 2 + *(it + 1)) {
    if (*it == IEEE_80211::kElemIdErp) {
      phy_mode = Metrics::kWiFiNetworkPhyMode11g;
      continue;  // NB: Continue to check for HT
    }
    if (*it == IEEE_80211::kElemIdHTCap || *it == IEEE_80211::kElemIdHTInfo) {
      phy_mode = Metrics::kWiFiNetworkPhyMode11n;
      break;
    }
  }

  return phy_mode;
}

}  // namespace shill
