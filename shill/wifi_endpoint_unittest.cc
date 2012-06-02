// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_endpoint.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/stl_util.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/ieee80211.h"
#include "shill/mock_log.h"
#include "shill/mock_wifi.h"
#include "shill/property_store_unittest.h"
#include "shill/refptr_types.h"
#include "shill/wpa_supplicant.h"

using std::map;
using std::set;
using std::string;
using std::vector;
using ::testing::_;
using testing::HasSubstr;
using ::testing::NiceMock;

namespace shill {

class WiFiEndpointTest : public PropertyStoreTest {
 public:
  WiFiEndpointTest() : wifi_(
      new NiceMock<MockWiFi>(
          control_interface(),
          dispatcher(),
          metrics(),
          manager(),
          "wifi",
          "aabbccddeeff",  // fake mac
          0)) {}
  virtual ~WiFiEndpointTest() {}

 protected:
  vector<string> make_string_vector1(const string &str1) {
    vector<string> strvec;
    strvec.push_back(str1);
    return strvec;
  }

  vector<string> make_string_vector2(const string &str1, const string &str2) {
    vector<string> strvec;
    strvec.push_back(str1);
    strvec.push_back(str2);
    return strvec;
  }

  map<string, ::DBus::Variant> make_key_management_args(
      vector<string> key_management_method_strings) {
    map<string, ::DBus::Variant> args;
    ::DBus::MessageIter writer;
    writer =
        args[wpa_supplicant::kSecurityMethodPropertyKeyManagement].writer();
    writer << key_management_method_strings;
    return args;
  }

  map<string, ::DBus::Variant> make_security_args(
      const string &security_protocol,
      const string &key_management_method) {
    map<string, ::DBus::Variant> args;
    ::DBus::MessageIter writer;
    writer = args[security_protocol].writer();
    writer <<
        make_key_management_args(make_string_vector1(key_management_method));
    return args;
  }

  const char *ParseSecurity(
    const map<string, ::DBus::Variant> &properties) {
    return WiFiEndpoint::ParseSecurity(properties);
  }

  void AddIE(uint8_t type, vector<uint8_t> *ies) {
    ies->push_back(type);           // type
    ies->push_back(1);              // length
    ies->push_back(0);              // data
  }

  void AddVendorIE(uint32_t oui, uint8_t vendor_type,
                   const vector<uint8_t> &data,
                   vector<uint8_t> *ies) {
    ies->push_back(IEEE_80211::kElemIdVendor);  // type
    ies->push_back(4 + data.size());            // length
    ies->push_back((oui >> 16) & 0xff);         // OUI MSByte
    ies->push_back((oui >> 8) & 0xff);          // OUI middle octet
    ies->push_back(oui & 0xff);                 // OUI LSByte
    ies->push_back(vendor_type);                // OUI Type
    ies->insert(ies->end(), data.begin(), data.end());
  }

  void AddWPSElement(uint16_t type, const string &value,
                     vector<uint8_t> *wps) {
    wps->push_back(type >> 8);                   // type MSByte
    wps->push_back(type);                        // type LSByte
    CHECK(value.size() < kuint16max);
    wps->push_back((value.size() >> 8) & 0xff);  // length MSByte
    wps->push_back(value.size() & 0xff);         // length LSByte
    wps->insert(wps->end(), value.begin(), value.end());
  }

  map<string, ::DBus::Variant> MakeBSSPropertiesWithIEs(
      const vector<uint8_t> &ies) {
    map<string, ::DBus::Variant> properties;
    ::DBus::MessageIter writer =
          properties[wpa_supplicant::kBSSPropertyIEs].writer();
    writer << ies;
    return properties;
  }

  WiFiEndpoint *MakeOpenEndpoint(ProxyFactory *proxy_factory,
                                 const WiFiRefPtr &wifi,
                                 const std::string &ssid,
                                 const std::string &bssid) {
    return WiFiEndpoint::MakeOpenEndpoint(
        proxy_factory, wifi, ssid, bssid, 0, 0);
  }

  scoped_refptr<MockWiFi> wifi() { return wifi_; }

 private:
  scoped_refptr<MockWiFi> wifi_;
};

TEST_F(WiFiEndpointTest, ParseKeyManagementMethodsEAP) {
  set<WiFiEndpoint::KeyManagement> parsed_methods;
  WiFiEndpoint::ParseKeyManagementMethods(
      make_key_management_args(make_string_vector1("something-eap")),
      &parsed_methods);
  EXPECT_TRUE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagement802_1x));
  EXPECT_FALSE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagementPSK));
}

TEST_F(WiFiEndpointTest, ParseKeyManagementMethodsPSK) {
  set<WiFiEndpoint::KeyManagement> parsed_methods;
  WiFiEndpoint::ParseKeyManagementMethods(
      make_key_management_args(make_string_vector1("something-psk")),
      &parsed_methods);
  EXPECT_TRUE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagementPSK));
  EXPECT_FALSE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagement802_1x));
}

TEST_F(WiFiEndpointTest, ParseKeyManagementMethodsEAPAndPSK) {
  set<WiFiEndpoint::KeyManagement> parsed_methods;
  WiFiEndpoint::ParseKeyManagementMethods(
      make_key_management_args(
          make_string_vector2("something-eap", "something-psk")),
      &parsed_methods);
  EXPECT_TRUE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagement802_1x));
  EXPECT_TRUE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagementPSK));
}

TEST_F(WiFiEndpointTest, ParseSecurityRSN802_1x) {
  EXPECT_STREQ(flimflam::kSecurity8021x,
               ParseSecurity(make_security_args("RSN", "something-eap")));
}

TEST_F(WiFiEndpointTest, ParseSecurityWPA802_1x) {
  EXPECT_STREQ(flimflam::kSecurity8021x,
               ParseSecurity(make_security_args("WPA", "something-eap")));
}

TEST_F(WiFiEndpointTest, ParseSecurityRSNPSK) {
  EXPECT_STREQ(flimflam::kSecurityRsn,
               ParseSecurity(make_security_args("RSN", "something-psk")));
}

TEST_F(WiFiEndpointTest, ParseSecurityWPAPSK) {
  EXPECT_STREQ(flimflam::kSecurityWpa,
               ParseSecurity(make_security_args("WPA", "something-psk")));
}

TEST_F(WiFiEndpointTest, ParseSecurityWEP) {
  map<string, ::DBus::Variant> top_params;
  top_params[wpa_supplicant::kPropertyPrivacy].writer().append_bool(true);
  EXPECT_STREQ(flimflam::kSecurityWep, ParseSecurity(top_params));
}

TEST_F(WiFiEndpointTest, ParseSecurityNone) {
  map<string, ::DBus::Variant> top_params;
  EXPECT_STREQ(flimflam::kSecurityNone, ParseSecurity(top_params));
}

TEST_F(WiFiEndpointTest, SSIDWithNull) {
  WiFiEndpointRefPtr endpoint =
      MakeOpenEndpoint(NULL, NULL, string(1, 0), "00:00:00:00:00:01");
  EXPECT_EQ("?", endpoint->ssid_string());
}

TEST_F(WiFiEndpointTest, DeterminePhyModeFromFrequency) {
  {
    map<string, ::DBus::Variant> properties;
    EXPECT_EQ(Metrics::kWiFiNetworkPhyMode11a,
              WiFiEndpoint::DeterminePhyModeFromFrequency(properties, 3200));
  }
  {
    map<string, ::DBus::Variant> properties;
    vector<uint32_t> rates(1, 22000000);
    ::DBus::MessageIter writer =
        properties[wpa_supplicant::kBSSPropertyRates].writer();
    writer << rates;
    EXPECT_EQ(Metrics::kWiFiNetworkPhyMode11b,
              WiFiEndpoint::DeterminePhyModeFromFrequency(properties, 2400));
  }
  {
    map<string, ::DBus::Variant> properties;
    vector<uint32_t> rates(1, 54000000);
    ::DBus::MessageIter writer =
        properties[wpa_supplicant::kBSSPropertyRates].writer();
    writer << rates;
    EXPECT_EQ(Metrics::kWiFiNetworkPhyMode11g,
              WiFiEndpoint::DeterminePhyModeFromFrequency(properties, 2400));
  }
}

TEST_F(WiFiEndpointTest, ParseIEs) {
  {
    vector<uint8_t> ies;
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ(Metrics::kWiFiNetworkPhyModeUndef, phy_mode);
  }
  {
    vector<uint8_t> ies;
    AddIE(IEEE_80211::kElemIdErp, &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ(Metrics::kWiFiNetworkPhyMode11g, phy_mode);
  }
  {
    vector<uint8_t> ies;
    AddIE(IEEE_80211::kElemIdHTCap, &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ(Metrics::kWiFiNetworkPhyMode11n, phy_mode);
  }
  {
    vector<uint8_t> ies;
    AddIE(IEEE_80211::kElemIdHTInfo, &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ(Metrics::kWiFiNetworkPhyMode11n, phy_mode);
  }
  {
    vector<uint8_t> ies;
    AddIE(IEEE_80211::kElemIdErp, &ies);
    AddIE(IEEE_80211::kElemIdHTCap, &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ(Metrics::kWiFiNetworkPhyMode11n, phy_mode);
  }
}

TEST_F(WiFiEndpointTest, ParseVendorIEs) {
  {
    ScopedMockLog log;
    EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                         HasSubstr("no room in IE for OUI and type field.")))
        .Times(1);
    vector<uint8_t> ies;
    AddIE(IEEE_80211::kElemIdVendor, &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
  }
  {
    vector<uint8_t> ies;
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ("", vendor_information.wps_manufacturer);
    EXPECT_EQ("", vendor_information.wps_model_name);
    EXPECT_EQ("", vendor_information.wps_model_number);
    EXPECT_EQ("", vendor_information.wps_device_name);
    EXPECT_EQ(0, vendor_information.oui_list.size());
  }
  {
    ScopedMockLog log;
    EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                         HasSubstr("IE extends past containing PDU"))).Times(1);
    vector<uint8_t> ies;
    AddVendorIE(0, 0, vector<uint8_t>(), &ies);
    ies.resize(ies.size() - 1);  // Cause an underrun in the data.
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
  }
  {
    vector<uint8_t> ies;
    const uint32_t kVendorOUI = 0xaabbcc;
    AddVendorIE(kVendorOUI, 0, vector<uint8_t>(), &ies);
    AddVendorIE(IEEE_80211::kOUIVendorMicrosoft, 0, vector<uint8_t>(), &ies);
    AddVendorIE(IEEE_80211::kOUIVendorEpigram, 0, vector<uint8_t>(), &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ("", vendor_information.wps_manufacturer);
    EXPECT_EQ("", vendor_information.wps_model_name);
    EXPECT_EQ("", vendor_information.wps_model_number);
    EXPECT_EQ("", vendor_information.wps_device_name);
    EXPECT_EQ(1, vendor_information.oui_list.size());
    EXPECT_FALSE(vendor_information.oui_list.find(kVendorOUI) ==
                 vendor_information.oui_list.end());

    WiFiEndpointRefPtr endpoint =
        MakeOpenEndpoint(NULL, NULL, string(1, 0), "00:00:00:00:00:01");
    endpoint->vendor_information_ = vendor_information;
    map<string, string> vendor_stringmap(endpoint->GetVendorInformation());
    EXPECT_FALSE(ContainsKey(vendor_stringmap, kVendorWPSManufacturerProperty));
    EXPECT_FALSE(ContainsKey(vendor_stringmap, kVendorWPSModelNameProperty));
    EXPECT_FALSE(ContainsKey(vendor_stringmap, kVendorWPSModelNumberProperty));
    EXPECT_FALSE(ContainsKey(vendor_stringmap, kVendorWPSDeviceNameProperty));
    EXPECT_EQ("aa-bb-cc", vendor_stringmap[kVendorOUIListProperty]);
  }
  {
    ScopedMockLog log;
    EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                         HasSubstr("WPS element extends past containing PDU")))
        .Times(1);
    vector<uint8_t> ies;
    vector<uint8_t> wps;
    AddWPSElement(IEEE_80211::kWPSElementManufacturer, "foo", &wps);
    wps.resize(wps.size() - 1);  // Cause an underrun in the data.
    AddVendorIE(IEEE_80211::kOUIVendorMicrosoft,
                IEEE_80211::kOUIMicrosoftWPS, wps, &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ("", vendor_information.wps_manufacturer);
  }
  {
    vector<uint8_t> ies;
    vector<uint8_t> wps;
    const string kManufacturer("manufacturer");
    const string kModelName("modelname");
    const string kModelNumber("modelnumber");
    const string kDeviceName("devicename");
    AddWPSElement(IEEE_80211::kWPSElementManufacturer, kManufacturer, &wps);
    AddWPSElement(IEEE_80211::kWPSElementModelName, kModelName, &wps);
    AddWPSElement(IEEE_80211::kWPSElementModelNumber, kModelNumber, &wps);
    AddWPSElement(IEEE_80211::kWPSElementDeviceName, kDeviceName, &wps);
    AddVendorIE(IEEE_80211::kOUIVendorMicrosoft,
                IEEE_80211::kOUIMicrosoftWPS, wps, &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ(kManufacturer, vendor_information.wps_manufacturer);
    EXPECT_EQ(kModelName, vendor_information.wps_model_name);
    EXPECT_EQ(kModelNumber, vendor_information.wps_model_number);
    EXPECT_EQ(kDeviceName, vendor_information.wps_device_name);

    WiFiEndpointRefPtr endpoint =
        MakeOpenEndpoint(NULL, NULL, string(1, 0), "00:00:00:00:00:01");
    endpoint->vendor_information_ = vendor_information;
    map<string, string> vendor_stringmap(endpoint->GetVendorInformation());
    EXPECT_EQ(kManufacturer, vendor_stringmap[kVendorWPSManufacturerProperty]);
    EXPECT_EQ(kModelName, vendor_stringmap[kVendorWPSModelNameProperty]);
    EXPECT_EQ(kModelNumber, vendor_stringmap[kVendorWPSModelNumberProperty]);
    EXPECT_EQ(kDeviceName, vendor_stringmap[kVendorWPSDeviceNameProperty]);
    EXPECT_FALSE(ContainsKey(vendor_stringmap, kVendorOUIListProperty));
  }
  {
    vector<uint8_t> ies;
    vector<uint8_t> wps;
    const string kManufacturer("manufacturer");
    const string kModelName("modelname");
    AddWPSElement(IEEE_80211::kWPSElementManufacturer, kManufacturer, &wps);
    wps.resize(wps.size() - 1);  // Insert a non-ASCII character in the WPS.
    wps.push_back(0x80);
    AddWPSElement(IEEE_80211::kWPSElementModelName, kModelName, &wps);
    AddVendorIE(IEEE_80211::kOUIVendorMicrosoft,
                IEEE_80211::kOUIMicrosoftWPS, wps, &ies);
    Metrics::WiFiNetworkPhyMode phy_mode = Metrics::kWiFiNetworkPhyModeUndef;
    WiFiEndpoint::VendorInformation vendor_information;
    WiFiEndpoint::ParseIEs(MakeBSSPropertiesWithIEs(ies),
                           &phy_mode, &vendor_information);
    EXPECT_EQ("", vendor_information.wps_manufacturer);
    EXPECT_EQ(kModelName, vendor_information.wps_model_name);
  }
}

TEST_F(WiFiEndpointTest, PropertiesChanged) {
  WiFiEndpointRefPtr endpoint =
      MakeOpenEndpoint(NULL, wifi(), "ssid", "00:00:00:00:00:01");
  map<string, ::DBus::Variant> changed_properties;
  ::DBus::MessageIter writer;
  int16_t signal_strength = 10;

  EXPECT_NE(signal_strength, endpoint->signal_strength());
  writer =
      changed_properties[wpa_supplicant::kBSSPropertySignal].writer();
  writer << signal_strength;

  EXPECT_CALL(*wifi(), NotifyEndpointChanged(_));
  endpoint->PropertiesChanged(changed_properties);
  EXPECT_EQ(signal_strength, endpoint->signal_strength());
}

}  // namespace shill
