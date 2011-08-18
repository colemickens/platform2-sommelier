// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi.h"

#include <map>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <dbus-c++/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/manager.h"
#include "shill/mock_device.h"
#include "shill/mock_supplicant_interface_proxy.h"
#include "shill/mock_supplicant_process_proxy.h"
#include "shill/nice_mock_control.h"
#include "shill/property_store_unittest.h"
#include "shill/proxy_factory.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DefaultValue;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Test;
using ::testing::Throw;

namespace shill {

class WiFiPropertyTest : public PropertyStoreTest {
 public:
  WiFiPropertyTest()
      : device_(new WiFi(&control_interface_, NULL, NULL, "wifi", 0)) {
  }
  virtual ~WiFiPropertyTest() {}

 protected:
  DeviceRefPtr device_;
};

TEST_F(WiFiPropertyTest, Contains) {
  EXPECT_TRUE(device_->store()->Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->store()->Contains(""));
}

TEST_F(WiFiPropertyTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_->store(),
                                            flimflam::kBgscanMethodProperty,
                                            PropertyStoreTest::kStringV,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(
        device_->store(),
        flimflam::kBgscanSignalThresholdProperty,
        PropertyStoreTest::kInt32V,
        &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_->store(),
                                            flimflam::kScanIntervalProperty,
                                            PropertyStoreTest::kUint16V,
                                            &error));
  }
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->store(),
                                             flimflam::kScanningProperty,
                                             PropertyStoreTest::kBoolV,
                                             &error));
    EXPECT_EQ(invalid_args_, error.name());
  }
}

class WiFiMainTest : public Test {
 public:
  WiFiMainTest()
      : manager_(&control_interface_, NULL, NULL),
        wifi_(new WiFi(&control_interface_, NULL, &manager_, kDeviceName, 0)),
        supplicant_process_proxy_(new NiceMock<MockSupplicantProcessProxy>()),
        supplicant_interface_proxy_(
            new NiceMock<MockSupplicantInterfaceProxy>(wifi_)),
        proxy_factory_(this) {
    ProxyFactory::set_factory(&proxy_factory_);
    ::testing::DefaultValue< ::DBus::Path>::Set("/default/path");
  }
  virtual ~WiFiMainTest() {
    // must Stop WiFi instance, to clear its list of services.
    // otherwise, the WiFi instance will not be deleted. (because
    // services reference a WiFi instance, creating a cycle.)
    wifi_->Stop();
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    TestProxyFactory(WiFiMainTest *test) : test_(test) {}

    virtual SupplicantProcessProxyInterface *CreateSupplicantProcessProxy(
        const char *dbus_path, const char *dbus_addr) {
      return test_->supplicant_process_proxy_.release();
    }

    virtual SupplicantInterfaceProxyInterface *CreateSupplicantInterfaceProxy(
        const WiFiRefPtr &wifi,
        const DBus::Path &object_path,
        const char *dbus_addr) {
      return test_->supplicant_interface_proxy_.release();
    }

   private:
    WiFiMainTest *test_;
  };

  const WiFi::EndpointMap &GetEndpointMap() {
    return wifi_->endpoint_by_bssid_;
  }
  const WiFi::ServiceMap &GetServiceMap() {
    return wifi_->service_by_private_id_;
  }
  // note: the tests need the proxies referenced by WiFi (not the
  // proxies instantiated by WiFiMainTest), to ensure that WiFi
  // sets up its proxies correctly.
  SupplicantProcessProxyInterface *GetSupplicantProcessProxy() {
    return wifi_->supplicant_process_proxy_.get();
  }
  SupplicantInterfaceProxyInterface *GetSupplicantInterfaceProxy() {
    return wifi_->supplicant_interface_proxy_.get();
  }
  void InitiateConnect(const WiFiService &service) {
    wifi_->ConnectTo(service);
  }
  void ReportBSS(const ::DBus::Path &bss_path,
                 const string &ssid,
                 const string &bssid,
                 int16_t signal_strength,
                 const char *mode);
  void ReportScanDone() {
    wifi_->ScanDoneTask();
  }
  void StartWiFi() {
    wifi_->Start();
  }
  void StopWiFi() {
    wifi_->Stop();
  }

 private:
  NiceMockControl control_interface_;
  Manager manager_;
  WiFiRefPtr wifi_;

  // protected fields interspersed between private fields, due to
  // initialization order
 protected:
  static const char kDeviceName[];
  static const char kNetworkModeAdHoc[];
  static const char kNetworkModeInfrastructure[];

  scoped_ptr<MockSupplicantProcessProxy> supplicant_process_proxy_;
  scoped_ptr<MockSupplicantInterfaceProxy> supplicant_interface_proxy_;

 private:
  TestProxyFactory proxy_factory_;

};

const char WiFiMainTest::kDeviceName[] = "wlan0";
const char WiFiMainTest::kNetworkModeAdHoc[] = "ad-hoc";
const char WiFiMainTest::kNetworkModeInfrastructure[] = "infrastructure";

void WiFiMainTest::ReportBSS(const ::DBus::Path &bss_path,
                             const string &ssid,
                             const string &bssid,
                             int16_t signal_strength,
                             const char *mode) {
  map<string, ::DBus::Variant> bss_properties;

  {
    DBus::MessageIter writer(bss_properties["SSID"].writer());
    writer << vector<uint8_t>(ssid.begin(), ssid.end());
  }
  {
    string bssid_nosep;
    vector<uint8_t> bssid_bytes;
    RemoveChars(bssid, ":", &bssid_nosep);
    base::HexStringToBytes(bssid_nosep, &bssid_bytes);

    DBus::MessageIter writer(bss_properties["BSSID"].writer());
    writer << bssid_bytes;
  }
  bss_properties["Signal"].writer().append_int16(signal_strength);
  bss_properties["Mode"].writer().append_string(mode);
  wifi_->BSSAdded(bss_path, bss_properties);
}

TEST_F(WiFiMainTest, ProxiesSetUpDuringStart) {
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);
  EXPECT_TRUE(GetSupplicantInterfaceProxy() == NULL);

  StartWiFi();
  EXPECT_FALSE(GetSupplicantProcessProxy() == NULL);
  EXPECT_FALSE(GetSupplicantInterfaceProxy() == NULL);
}

TEST_F(WiFiMainTest, CleanStart) {
  EXPECT_CALL(*supplicant_process_proxy_, CreateInterface(_));
  EXPECT_CALL(*supplicant_process_proxy_, GetInterface(_))
      .Times(AnyNumber())
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.InterfaceUnknown",
              "test threw fi.w1.wpa_supplicant1.InterfaceUnknown")));
  EXPECT_CALL(*supplicant_interface_proxy_, Scan(_));
  StartWiFi();
}

TEST_F(WiFiMainTest, Restart) {
  EXPECT_CALL(*supplicant_process_proxy_, CreateInterface(_))
      .Times(AnyNumber())
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.InterfaceExists",
              "test thew fi.w1.wpa_supplicant1.InterfaceExists")));
  EXPECT_CALL(*supplicant_process_proxy_, GetInterface(_));
  EXPECT_CALL(*supplicant_interface_proxy_, Scan(_));
  StartWiFi();
}

TEST_F(WiFiMainTest, StartClearsState) {
  EXPECT_CALL(*supplicant_interface_proxy_, RemoveAllNetworks());
  EXPECT_CALL(*supplicant_interface_proxy_, FlushBSS(_));
  StartWiFi();
}

TEST_F(WiFiMainTest, ScanResults) {
  StartWiFi();
  ReportBSS("bss0", "ssid0", "00:00:00:00:00:00", 0, kNetworkModeAdHoc);
  ReportBSS(
      "bss1", "ssid1", "00:00:00:00:00:01", 1, kNetworkModeInfrastructure);
  ReportBSS(
      "bss2", "ssid2", "00:00:00:00:00:02", 2, kNetworkModeInfrastructure);
  ReportBSS(
      "bss3", "ssid3", "00:00:00:00:00:03", 3, kNetworkModeInfrastructure);
  ReportBSS("bss4", "ssid4", "00:00:00:00:00:04", 4, kNetworkModeAdHoc);
  EXPECT_EQ(5, GetEndpointMap().size());
}

TEST_F(WiFiMainTest, ScanResultsWithUpdates) {
  StartWiFi();
  ReportBSS("bss0", "ssid0", "00:00:00:00:00:00", 0, kNetworkModeAdHoc);
  ReportBSS(
      "bss1", "ssid1", "00:00:00:00:00:01", 1, kNetworkModeInfrastructure);
  ReportBSS(
      "bss2", "ssid2", "00:00:00:00:00:02", 2, kNetworkModeInfrastructure);
  ReportBSS(
      "bss1", "ssid1", "00:00:00:00:00:01", 3, kNetworkModeInfrastructure);
  ReportBSS("bss0", "ssid0", "00:00:00:00:00:00", 4, kNetworkModeAdHoc);
  EXPECT_EQ(3, GetEndpointMap().size());
  ASSERT_TRUE(ContainsKey(GetEndpointMap(), "000000000000"));
  EXPECT_EQ(4, GetEndpointMap().find("000000000000")->second->
            signal_strength());
}

TEST_F(WiFiMainTest, ScanCompleted) {
  StartWiFi();
  ReportBSS("bss0", "ssid0", "00:00:00:00:00:00", 0, kNetworkModeAdHoc);
  ReportBSS(
      "bss1", "ssid1", "00:00:00:00:00:01", 1, kNetworkModeInfrastructure);
  ReportBSS(
      "bss2", "ssid2", "00:00:00:00:00:02", 2, kNetworkModeInfrastructure);
  ReportScanDone();
  EXPECT_EQ(3, GetServiceMap().size());
}

TEST_F(WiFiMainTest, Connect) {
  MockSupplicantInterfaceProxy &supplicant_interface_proxy =
      *supplicant_interface_proxy_;

  StartWiFi();
  ReportBSS("bss0", "ssid0", "00:00:00:00:00:00", 0, kNetworkModeAdHoc);
  ReportScanDone();

  {
    InSequence s;
    DBus::Path fake_path("/fake/path");

    EXPECT_CALL(supplicant_interface_proxy, AddNetwork(_))
        .WillOnce(Return(fake_path));
    EXPECT_CALL(supplicant_interface_proxy, SelectNetwork(fake_path));
    InitiateConnect(*(GetServiceMap().begin()->second));
  }
}

}  // namespace shill
