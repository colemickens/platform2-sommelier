// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_endpoint.h"

#include <algorithm>

#include <base/stl_util.h>
#include <base/stringprintf.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/ieee80211.h"
#include "shill/logging.h"
#include "shill/proxy_factory.h"
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

  Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
  if (!ParseIEs(properties, &phy_mode, &vendor_information_)) {
    phy_mode = DeterminePhyModeFromFrequency(properties, frequency_);
  }
  physical_mode_ = phy_mode;

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


map<string, string> WiFiEndpoint::GetVendorInformation() const {
  map<string, string> vendor_information;
  if (!vendor_information_.wps_manufacturer.empty()) {
    vendor_information[kVendorWPSManufacturerProperty] =
        vendor_information_.wps_manufacturer;
  }
  if (!vendor_information_.wps_model_name.empty()) {
    vendor_information[kVendorWPSModelNameProperty] =
        vendor_information_.wps_model_name;
  }
  if (!vendor_information_.wps_model_number.empty()) {
    vendor_information[kVendorWPSModelNumberProperty] =
        vendor_information_.wps_model_number;
  }
  if (!vendor_information_.wps_device_name.empty()) {
    vendor_information[kVendorWPSDeviceNameProperty] =
        vendor_information_.wps_device_name;
  }
  if (!vendor_information_.oui_list.empty()) {
    vector<string> oui_list;
    set<uint32_t>::const_iterator it;
    for (it = vendor_information_.oui_list.begin();
         it != vendor_information_.oui_list.end();
         ++it) {
      oui_list.push_back(
          StringPrintf("%02x-%02x-%02x",
              *it >> 16, (*it >> 8) & 0xff, *it & 0xff));
    }
    vendor_information[kVendorOUIListProperty] =
        JoinString(oui_list, ' ');
  }
  return vendor_information;
}

// static
uint32_t WiFiEndpoint::ModeStringToUint(const string &mode_string) {
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
Metrics::WiFiNetworkPhyMode WiFiEndpoint::DeterminePhyModeFromFrequency(
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
bool WiFiEndpoint::ParseIEs(
    const map<string, ::DBus::Variant> &properties,
    Metrics::WiFiNetworkPhyMode *phy_mode,
    VendorInformation *vendor_information) {

  map<string, ::DBus::Variant>::const_iterator ies_property =
      properties.find(wpa_supplicant::kBSSPropertyIEs);
  if (ies_property == properties.end()) {
    SLOG(WiFi, 2) << __func__ << ": No IE property in BSS.";
    return false;
  }

  vector<uint8_t> ies = ies_property->second.operator vector<uint8_t>();


  // Format of an information element:
  //    1       1          1 - 252
  // +------+--------+----------------+
  // | Type | Length | Data           |
  // +------+--------+----------------+
  *phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
  bool found_ht = false;
  bool found_erp = false;
  int ie_len = 0;
  vector<uint8_t>::iterator it;
  for (it = ies.begin();
       std::distance(it, ies.end()) > 1;  // Ensure Length field is within PDU.
       it += ie_len) {
    ie_len = 2 + *(it + 1);
    if (std::distance(it, ies.end()) < ie_len) {
      LOG(ERROR) << __func__ << ": IE extends past containing PDU.";
      break;
    }
    switch (*it) {
      case IEEE_80211::kElemIdErp:
        if (!found_ht) {
          *phy_mode = Metrics::kWiFiNetworkPhyMode11g;
        }
        found_erp = true;
        break;
      case IEEE_80211::kElemIdHTCap:
      case IEEE_80211::kElemIdHTInfo:
        *phy_mode = Metrics::kWiFiNetworkPhyMode11n;
        found_ht = true;
        break;
      case IEEE_80211::kElemIdVendor:
        ParseVendorIE(it + 2, it + ie_len, vendor_information);
        break;
    }
  }
  return found_ht || found_erp;
}

// static
void WiFiEndpoint::ParseVendorIE(vector<uint8_t>::const_iterator ie,
                                 vector<uint8_t>::const_iterator end,
                                 VendorInformation *vendor_information) {
  // Format of an vendor-specific information element (with type
  // and length field for the IE removed by the caller):
  //        3           1       1 - 248
  // +------------+----------+----------------+
  // | OUI        | OUI Type | Data           |
  // +------------+----------+----------------+

  if (std::distance(ie, end) < 4) {
    LOG(ERROR) << __func__ << ": no room in IE for OUI and type field.";
    return;
  }
  uint32_t oui = (*ie << 16) | (*(ie + 1) << 8) | *(ie + 2);
  uint8_t oui_type = *(ie + 3);
  ie += 4;

  if (oui == IEEE_80211::kOUIVendorMicrosoft &&
      oui_type == IEEE_80211::kOUIMicrosoftWPS) {
    // Format of a WPS data element:
    //    2       2
    // +------+--------+----------------+
    // | Type | Length | Data           |
    // +------+--------+----------------+
    while (std::distance(ie, end) >= 4) {
      int element_type = (*ie << 8) | *(ie + 1);
      int element_length = (*(ie + 2) << 8) | *(ie + 3);
      ie += 4;
      if (std::distance(ie, end) < element_length) {
        LOG(ERROR) << __func__ << ": WPS element extends past containing PDU.";
        break;
      }
      string s(ie, ie + element_length);
      if (IsStringASCII(s)) {
        switch (element_type) {
          case IEEE_80211::kWPSElementManufacturer:
            vendor_information->wps_manufacturer = s;
            break;
          case IEEE_80211::kWPSElementModelName:
            vendor_information->wps_model_name = s;
            break;
          case IEEE_80211::kWPSElementModelNumber:
            vendor_information->wps_model_number = s;
            break;
          case IEEE_80211::kWPSElementDeviceName:
            vendor_information->wps_device_name = s;
            break;
        }
      }
      ie += element_length;
    }
  } else if (oui != IEEE_80211::kOUIVendorEpigram &&
             oui != IEEE_80211::kOUIVendorMicrosoft) {
    vendor_information->oui_list.insert(oui);
  }
}

}  // namespace shill
