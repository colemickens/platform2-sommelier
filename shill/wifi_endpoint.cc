// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/stringprintf.h>
#include <base/string_number_conversions.h>

#include "shill/wifi_endpoint.h"

using std::string;

namespace shill {

const char WiFiEndpoint::kSupplicantPropertySSID[]   = "SSID";
const char WiFiEndpoint::kSupplicantPropertyBSSID[]  = "BSSID";
const char WiFiEndpoint::kSupplicantPropertySignal[] = "Signal";
const char WiFiEndpoint::kSupplicantPropertyMode[]   = "Mode";

const char WiFiEndpoint::kSupplicantNetworkModeInfrastructure[] =
    "infrastructure";
const char WiFiEndpoint::kSupplicantNetworkModeAdHoc[]       = "ad-hoc";
const char WiFiEndpoint::kSupplicantNetworkModeAccessPoint[] = "ap";

const uint32_t WiFiEndpoint::kSupplicantNetworkModeInfrastructureInt = 0;
const uint32_t WiFiEndpoint::kSupplicantNetworkModeAdHocInt          = 1;
const uint32_t WiFiEndpoint::kSupplicantNetworkModeAccessPointInt    = 2;

WiFiEndpoint::WiFiEndpoint(
    const std::map<string, ::DBus::Variant> &properties) {
  // XXX will segfault on missing properties
  ssid_ =
      properties.find(kSupplicantPropertySSID)->second.
      operator std::vector<uint8_t>();
  bssid_ =
      properties.find(kSupplicantPropertyBSSID)->second.
      operator std::vector<uint8_t>();
  signal_strength_ =
      properties.find(kSupplicantPropertySignal)->second;
  network_mode_ = parse_mode(
      properties.find(kSupplicantPropertyMode)->second);

  if (network_mode_ < 0) {
    // XXX log error?
  }

  ssid_string_ = string(ssid_.begin(), ssid_.end());
  ssid_hex_ = base::HexEncode(&(*ssid_.begin()), ssid_.size());
  bssid_string_ = StringPrintf("%02x:%02x:%02x:%02x:%02x:%02x",
                               bssid_[0], bssid_[1], bssid_[2],
                               bssid_[3], bssid_[4], bssid_[5]);
  bssid_hex_ = base::HexEncode(&(*bssid_.begin()), bssid_.size());
}

WiFiEndpoint::~WiFiEndpoint() {}

const std::vector<uint8_t> &WiFiEndpoint::ssid() const {
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

uint32_t WiFiEndpoint::network_mode() const {
  return network_mode_;
}

int32_t WiFiEndpoint::parse_mode(const std::string &mode_string) {
  if (mode_string == kSupplicantNetworkModeInfrastructure) {
    return kSupplicantNetworkModeInfrastructureInt;
  } else if (mode_string == kSupplicantNetworkModeAdHoc) {
    return kSupplicantNetworkModeAdHocInt;
  } else if (mode_string == kSupplicantNetworkModeAccessPoint) {
    return kSupplicantNetworkModeAccessPointInt;
  } else {
    return -1;
  }
}

}  // namespace shill
