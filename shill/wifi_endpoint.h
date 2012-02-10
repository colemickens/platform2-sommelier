// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_ENDPOINT_
#define SHILL_WIFI_ENDPOINT_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <dbus-c++/dbus.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/endpoint.h"
#include "shill/event_dispatcher.h"

namespace shill {

class WiFiEndpoint : public Endpoint {
 public:
  WiFiEndpoint(const std::map<std::string, ::DBus::Variant> &properties);
  virtual ~WiFiEndpoint();

  // Maps mode strings from flimflam's nomenclature, as defined
  // in chromeos/dbus/service_constants.h, to uints used by supplicant
  static uint32_t ModeStringToUint(const std::string &mode_string);

  const std::vector<uint8_t> &ssid() const;
  const std::string &ssid_string() const;
  const std::string &ssid_hex() const;
  const std::string &bssid_string() const;
  const std::string &bssid_hex() const;
  int16_t signal_strength() const;
  uint16 frequency() const;
  const std::string &network_mode() const;
  const std::string &security_mode() const;

 private:
  friend class WiFiEndpointTest;
  friend class WiFiMainTest;  // for MakeOpenEndpoint
  friend class WiFiServiceTest;  // for MakeOpenEndpoint
  FRIEND_TEST(WiFiEndpointTest, SSIDWithNull);  // for MakeOpenEndpoint
  // these test cases need access to the KeyManagement enum
  FRIEND_TEST(WiFiEndpointTest, ParseKeyManagementMethodsEAP);
  FRIEND_TEST(WiFiEndpointTest, ParseKeyManagementMethodsPSK);
  FRIEND_TEST(WiFiEndpointTest, ParseKeyManagementMethodsEAPAndPSK);

  enum KeyManagement {
    kKeyManagement802_1x,
    kKeyManagementPSK
  };

  // Build a simple WiFiEndpoint, for testing purposes.
  static WiFiEndpoint *MakeOpenEndpoint(
      const std::string &ssid, const std::string &bssid);
  // Maps mode strings from supplicant into flimflam's nomenclature, as defined
  // in chromeos/dbus/service_constants.h
  static const char *ParseMode(const std::string &mode_string);
  // Parses an Endpoint's properties to identify approprirate flimflam
  // security property value, as defined in chromeos/dbus/service_constants.h
  static const char *ParseSecurity(
      const std::map<std::string, ::DBus::Variant> &properties);
  // Parses and Endpoint's properties' "RSN" or "WPA" sub-dictionary, to
  // identify supported key management methods (802.1x or PSK).
  static void ParseKeyManagementMethods(
      const std::map<std::string, ::DBus::Variant> &security_method_properties,
      std::set<KeyManagement> *key_management_methods);

  // TODO(quiche): make const?
  std::vector<uint8_t> ssid_;
  std::vector<uint8_t> bssid_;
  std::string ssid_string_;
  std::string ssid_hex_;
  std::string bssid_string_;
  std::string bssid_hex_;
  int16_t signal_strength_;
  uint16 frequency_;
  // network_mode_ and security_mode_ are represented as flimflam names
  // (not necessarily the same as wpa_supplicant names)
  std::string network_mode_;
  std::string security_mode_;

  DISALLOW_COPY_AND_ASSIGN(WiFiEndpoint);
};

}  // namespace shill

#endif  // SHILL_WIFI_ENDPOINT_
