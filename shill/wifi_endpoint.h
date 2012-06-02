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
#include "shill/metrics.h"
#include "shill/refptr_types.h"

namespace shill {

class ProxyFactory;
class SupplicantBSSProxyInterface;

class WiFiEndpoint : public Endpoint {
 public:
  struct VendorInformation {
    std::string wps_manufacturer;
    std::string wps_model_name;
    std::string wps_model_number;
    std::string wps_device_name;
    std::set<uint32_t> oui_list;
  };
  WiFiEndpoint(ProxyFactory *proxy_factory,
               const WiFiRefPtr &device,
               const std::string &rpc_id,
               const std::map<std::string, ::DBus::Variant> &properties);
  virtual ~WiFiEndpoint();

  // Set up RPC channel. Broken out from the ctor, so that WiFi can
  // look over the Endpoint details before commiting to setting up
  // RPC.
  virtual void Start();

  // Called by SupplicantBSSProxy, in response to events from
  // wpa_supplicant.
  void PropertiesChanged(
      const std::map<std::string, ::DBus::Variant> &properties);

  // Maps mode strings from flimflam's nomenclature, as defined
  // in chromeos/dbus/service_constants.h, to uints used by supplicant
  static uint32_t ModeStringToUint(const std::string &mode_string);

  // Returns a stringmap containing information gleaned about the
  // vendor of this AP.
  std::map<std::string, std::string> GetVendorInformation() const;

  const std::vector<uint8_t> &ssid() const;
  const std::string &ssid_string() const;
  const std::string &ssid_hex() const;
  const std::string &bssid_string() const;
  const std::string &bssid_hex() const;
  int16_t signal_strength() const;
  uint16 frequency() const;
  uint16 physical_mode() const;
  const std::string &network_mode() const;
  const std::string &security_mode() const;
  const VendorInformation &vendor_information() const;

 private:
  friend class WiFiEndpointTest;
  friend class WiFiObjectTest;  // for MakeOpenEndpoint
  friend class WiFiServiceTest;  // for MakeOpenEndpoint
  // these test cases need access to the KeyManagement enum
  FRIEND_TEST(WiFiEndpointTest, DeterminePhyModeFromFrequency);
  FRIEND_TEST(WiFiEndpointTest, ParseIEs);
  FRIEND_TEST(WiFiEndpointTest, ParseKeyManagementMethodsEAP);
  FRIEND_TEST(WiFiEndpointTest, ParseKeyManagementMethodsPSK);
  FRIEND_TEST(WiFiEndpointTest, ParseKeyManagementMethodsEAPAndPSK);
  FRIEND_TEST(WiFiEndpointTest, ParseVendorIEs);
  FRIEND_TEST(WiFiServiceUpdateFromEndpointsTest, EndpointModified);

  enum KeyManagement {
    kKeyManagement802_1x,
    kKeyManagementPSK
  };

  // Build a simple WiFiEndpoint, for testing purposes.
  static WiFiEndpoint *MakeOpenEndpoint(ProxyFactory *proxy_factory,
                                        const WiFiRefPtr &wifi,
                                        const std::string &ssid,
                                        const std::string &bssid,
                                        uint16 frequency,
                                        int16 signal_dbm);
  // Maps mode strings from supplicant into flimflam's nomenclature, as defined
  // in chromeos/dbus/service_constants.h
  static const char *ParseMode(const std::string &mode_string);
  // Parses an Endpoint's properties to identify approprirate flimflam
  // security property value, as defined in chromeos/dbus/service_constants.h
  static const char *ParseSecurity(
      const std::map<std::string, ::DBus::Variant> &properties);
  // Parses an Endpoint's properties' "RSN" or "WPA" sub-dictionary, to
  // identify supported key management methods (802.1x or PSK).
  static void ParseKeyManagementMethods(
      const std::map<std::string, ::DBus::Variant> &security_method_properties,
      std::set<KeyManagement> *key_management_methods);
  // Determine the negotiated operating mode for the channel by looking at
  // the information elements, frequency and data rates.  The information
  // elements and data rates live in |properties|.
  static Metrics::WiFiNetworkPhyMode DeterminePhyModeFromFrequency(
      const std::map<std::string, ::DBus::Variant> &properties,
      uint16 frequency);
  // Parse information elements to determine the physical mode and vendor
  // information associated with the AP.  Returns true if a physical mode
  // was determined from the IE elements, false otherwise.
  static bool ParseIEs(const std::map<std::string, ::DBus::Variant> &properties,
                       Metrics::WiFiNetworkPhyMode *phy_mode,
                       VendorInformation *vendor_information);
  // Parse a single vendor information element.
  static void ParseVendorIE(std::vector<uint8_t>::const_iterator ie,
                            std::vector<uint8_t>::const_iterator end,
                            VendorInformation *vendor_information);

  // TODO(quiche): make const?
  std::vector<uint8_t> ssid_;
  std::vector<uint8_t> bssid_;
  std::string ssid_string_;
  std::string ssid_hex_;
  std::string bssid_string_;
  std::string bssid_hex_;
  int16 signal_strength_;
  uint16 frequency_;
  uint16 physical_mode_;
  // network_mode_ and security_mode_ are represented as flimflam names
  // (not necessarily the same as wpa_supplicant names)
  std::string network_mode_;
  std::string security_mode_;
  VendorInformation vendor_information_;

  ProxyFactory *proxy_factory_;
  WiFiRefPtr device_;
  std::string rpc_id_;
  scoped_ptr<SupplicantBSSProxyInterface> supplicant_bss_proxy_;

  DISALLOW_COPY_AND_ASSIGN(WiFiEndpoint);
};

}  // namespace shill

#endif  // SHILL_WIFI_ENDPOINT_
