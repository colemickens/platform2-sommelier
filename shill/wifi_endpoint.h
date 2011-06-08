// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_ENDPOINT_
#define SHILL_WIFI_ENDPOINT_

#include <map>
#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <dbus-c++/dbus.h>

#include "shill/endpoint.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class WiFiEndpoint : public Endpoint {
 public:
  WiFiEndpoint(const std::map<std::string, ::DBus::Variant> &properties);
  virtual ~WiFiEndpoint();
  const std::vector<uint8_t> &ssid() const;
  const std::string &ssid_string() const;
  const std::string &ssid_hex() const;
  const std::string &bssid_string() const;
  const std::string &bssid_hex() const;
  int16_t signal_strength() const;
  uint32_t network_mode() const;

 private:
  static const uint32_t kSupplicantNetworkModeInfrastructureInt;
  static const uint32_t kSupplicantNetworkModeAdHocInt;
  static const uint32_t kSupplicantNetworkModeAccessPointInt;

  static const char kSupplicantPropertySSID[];
  static const char kSupplicantPropertyBSSID[];
  static const char kSupplicantPropertySignal[];
  static const char kSupplicantPropertyMode[];
  static const char kSupplicantNetworkModeInfrastructure[];
  static const char kSupplicantNetworkModeAdHoc[];
  static const char kSupplicantNetworkModeAccessPoint[];

  static int32_t parse_mode(const std::string &mode_string);

  std::vector<uint8_t> ssid_;
  std::vector<uint8_t> bssid_;
  std::string ssid_string_;
  std::string ssid_hex_;
  std::string bssid_string_;
  std::string bssid_hex_;
  int16_t signal_strength_;
  uint32_t network_mode_;

  DISALLOW_COPY_AND_ASSIGN(WiFiEndpoint);
};

}  // namespace shill

#endif  // SHILL_WIFI_ENDPOINT_
