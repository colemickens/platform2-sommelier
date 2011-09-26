// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi.h"

#include <netinet/ether.h>
#include <linux/if.h>
#include <sys/socket.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.

#include <map>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/dbus_adaptor.h"
#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/mock_device.h"
#include "shill/mock_dhcp_config.h"
#include "shill/mock_dhcp_provider.h"
#include "shill/mock_manager.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/mock_supplicant_interface_proxy.h"
#include "shill/mock_supplicant_process_proxy.h"
#include "shill/nice_mock_control.h"
#include "shill/property_store_unittest.h"
#include "shill/proxy_factory.h"
#include "shill/shill_event.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi.h"
#include "shill/wifi_service.h"

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
      : device_(new WiFi(control_interface(), NULL, NULL, "wifi", "", 0)) {
  }
  virtual ~WiFiPropertyTest() {}

 protected:
  DeviceRefPtr device_;
};

TEST_F(WiFiPropertyTest, Contains) {
  EXPECT_TRUE(device_->store().Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->store().Contains(""));
}

TEST_F(WiFiPropertyTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                            flimflam::kBgscanMethodProperty,
                                            PropertyStoreTest::kStringV,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(
        device_->mutable_store(),
        flimflam::kBgscanSignalThresholdProperty,
        PropertyStoreTest::kInt32V,
        &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                            flimflam::kScanIntervalProperty,
                                            PropertyStoreTest::kUint16V,
                                            &error));
  }
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                             flimflam::kScanningProperty,
                                             PropertyStoreTest::kBoolV,
                                             &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
}

class WiFiMainTest : public Test {
 public:
  WiFiMainTest()
      : manager_(&control_interface_, NULL, NULL),
        wifi_(new WiFi(&control_interface_,
                       &dispatcher_,
                       &manager_,
                       kDeviceName,
                       kDeviceAddress,
                       0)),
        supplicant_process_proxy_(new NiceMock<MockSupplicantProcessProxy>()),
        supplicant_interface_proxy_(
            new NiceMock<MockSupplicantInterfaceProxy>(wifi_)),
        dhcp_config_(new MockDHCPConfig(&control_interface_,
                                        &dispatcher_,
                                        &dhcp_provider_,
                                        kDeviceName,
                                        &glib_)),
        proxy_factory_(this) {
    ProxyFactory::set_factory(&proxy_factory_);
    ::testing::DefaultValue< ::DBus::Path>::Set("/default/path");
  }

  virtual void SetUp() {
    static_cast<Device *>(wifi_)->rtnl_handler_ = &rtnl_handler_;
    wifi_->set_dhcp_provider(&dhcp_provider_);
  }

  virtual void TearDown() {
    // must Stop WiFi instance, to clear its list of services.
    // otherwise, the WiFi instance will not be deleted. (because
    // services reference a WiFi instance, creating a cycle.)
    wifi_->Stop();
    wifi_->set_dhcp_provider(NULL);
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(WiFiMainTest *test) : test_(test) {}

    virtual SupplicantProcessProxyInterface *CreateSupplicantProcessProxy(
        const char */*dbus_path*/, const char */*dbus_addr*/) {
      return test_->supplicant_process_proxy_.release();
    }

    virtual SupplicantInterfaceProxyInterface *CreateSupplicantInterfaceProxy(
        const WiFiRefPtr &/*wifi*/,
        const DBus::Path &/*object_path*/,
        const char */*dbus_addr*/) {
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
  void InitiateConnect(WiFiService *service) {
    wifi_->ConnectTo(service);
  }
  bool IsLinkUp() {
    return wifi_->link_up_;
  }
  void ReportBSS(const ::DBus::Path &bss_path,
                 const string &ssid,
                 const string &bssid,
                 int16_t signal_strength,
                 const char *mode);
  void ReportLinkUp() {
    wifi_->LinkEvent(IFF_LOWER_UP, IFF_LOWER_UP);
  }
  void ReportScanDone() {
    wifi_->ScanDoneTask();
  }
  void StartWiFi() {
    wifi_->Start();
  }
  void StopWiFi() {
    wifi_->Stop();
  }
  void GetOpenService(const char *service_type,
                      const char *ssid,
                      const char *mode,
                      Error &result) {
    return GetService(service_type, ssid, mode, NULL, NULL, result);
  }
  void GetService(const char *service_type,
                  const char *ssid,
                  const char *mode,
                  const char *security,
                  const char *passphrase,
                  Error &result) {
    map<string, ::DBus::Variant> args;
    Error e;
    KeyValueStore args_kv;

    // in general, we want to avoid D-Bus specific code for any RPCs
    // that come in via adaptors. we make an exception here, because
    // calls to GetWifiService are rerouted from the Manager object to
    // the Wifi class.
    if (service_type != NULL)
      args[flimflam::kTypeProperty].writer().append_string(service_type);
    if (ssid != NULL)
      args[flimflam::kSSIDProperty].writer().append_string(ssid);
    if (mode != NULL)
      args[flimflam::kModeProperty].writer().append_string(mode);
    if (security != NULL)
      args[flimflam::kSecurityProperty].writer().append_string(security);
    if (passphrase != NULL)
      args[flimflam::kPassphraseProperty].writer().append_string(passphrase);

    DBusAdaptor::ArgsToKeyValueStore(args, &args_kv, &e);
    wifi_->GetService(args_kv, &result);
  }
  MockManager *manager() {
    return &manager_;
  }
  const WiFiConstRefPtr wifi() const {
    return wifi_;
  }

  EventDispatcher dispatcher_;
  NiceMock<MockRTNLHandler> rtnl_handler_;

 private:
  NiceMockControl control_interface_;
  MockGLib glib_;
  MockManager manager_;
  WiFiRefPtr wifi_;

  // protected fields interspersed between private fields, due to
  // initialization order
 protected:
  static const char kDeviceName[];
  static const char kDeviceAddress[];
  static const char kNetworkModeAdHoc[];
  static const char kNetworkModeInfrastructure[];

  scoped_ptr<MockSupplicantProcessProxy> supplicant_process_proxy_;
  scoped_ptr<MockSupplicantInterfaceProxy> supplicant_interface_proxy_;
  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;

 private:
  TestProxyFactory proxy_factory_;
};

const char WiFiMainTest::kDeviceName[] = "wlan0";
const char WiFiMainTest::kDeviceAddress[] = "00:01:02:03:04:05";
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
  dispatcher_.DispatchPendingEvents();
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
  dispatcher_.DispatchPendingEvents();
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
  EXPECT_CALL(*manager(), RegisterService(_))
      .Times(3);
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
    WiFiService *service(GetServiceMap().begin()->second);

    EXPECT_CALL(supplicant_interface_proxy, AddNetwork(_))
        .WillOnce(Return(fake_path));
    EXPECT_CALL(supplicant_interface_proxy, SelectNetwork(fake_path));
    InitiateConnect(service);
    EXPECT_EQ(static_cast<Service *>(service),
              wifi()->selected_service_.get());
  }
}

TEST_F(WiFiMainTest, LinkEvent) {
  EXPECT_FALSE(IsLinkUp());
  EXPECT_CALL(dhcp_provider_, CreateConfig(_)).
      WillOnce(Return(dhcp_config_));
  ReportLinkUp();
}

TEST_F(WiFiMainTest, Stop) {
  {
    InSequence s;

    StartWiFi();
    ReportBSS("bss0", "ssid0", "00:00:00:00:00:00", 0, kNetworkModeAdHoc);
    ReportScanDone();
    EXPECT_CALL(dhcp_provider_, CreateConfig(_)).
        WillOnce(Return(dhcp_config_));
    ReportLinkUp();
  }

  {
    EXPECT_CALL(*manager(), DeregisterService(_));
    StopWiFi();
  }
}

TEST_F(WiFiMainTest, GetWifiServiceOpen) {
  Error e;
  GetOpenService("wifi", "an_ssid", "managed", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceOpenNoType) {
  Error e;
  GetOpenService(NULL, "an_ssid", "managed", e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("must specify service type", e.message());
}

TEST_F(WiFiMainTest, GetWifiServiceOpenNoSSID) {
  Error e;
  GetOpenService("wifi", NULL, "managed", e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("must specify SSID", e.message());
}

TEST_F(WiFiMainTest, GetWifiServiceOpenLongSSID) {
  Error e;
  GetOpenService(
      "wifi", "123456789012345678901234567890123", "managed", e);
  EXPECT_EQ(Error::kInvalidNetworkName, e.type());
  EXPECT_EQ("SSID is too long", e.message());
}

TEST_F(WiFiMainTest, GetWifiServiceOpenShortSSID) {
  Error e;
  GetOpenService("wifi", "", "managed", e);
  EXPECT_EQ(Error::kInvalidNetworkName, e.type());
  EXPECT_EQ("SSID is too short", e.message());
}

TEST_F(WiFiMainTest, GetWifiServiceOpenBadMode) {
  Error e;
  GetOpenService("wifi", "an_ssid", "ad-hoc", e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_EQ("service mode is unsupported", e.message());
}

TEST_F(WiFiMainTest, GetWifiServiceOpenNoMode) {
  Error e;
  GetOpenService("wifi", "an_ssid", NULL, e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceRSN) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "rsn", "secure password", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceRSNNoPassword) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "rsn", NULL, e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("must specify passphrase", e.message());
}

TEST_F(WiFiMainTest, GetWifiServiceBadSecurity) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "rot-13", NULL, e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_EQ("security mode is unsupported", e.message());
}

TEST_F(WiFiMainTest, GetWifiServiceWEPNoPassword) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", NULL, e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("must specify passphrase", e.message());
}

TEST_F(WiFiMainTest, GetWifiServiceWEPEmptyPassword) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "", e);
  EXPECT_EQ(Error::kInvalidPassphrase, e.type());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP40ASCII) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "abcde", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP104ASCII) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "abcdefghijklm", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP40ASCIIWithKeyIndex) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "0:abcdefghijklm", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP40Hex) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "0102030405", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP40HexBadPassphrase) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "O102030405", e);
  EXPECT_EQ(Error::kInvalidPassphrase, e.type());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP40HexWithKeyIndexBadPassphrase) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "1:O102030405", e);
  EXPECT_EQ(Error::kInvalidPassphrase, e.type());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP40HexWithKeyIndexAndBaseBadPassphrase) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "1:0xO102030405", e);
  EXPECT_EQ(Error::kInvalidPassphrase, e.type());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP40HexWithBaseBadPassphrase) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep", "0xO102030405", e);
  EXPECT_EQ(Error::kInvalidPassphrase, e.type());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP104Hex) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep",
             "0102030405060708090a0b0c0d", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP104HexUppercase) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep",
             "0102030405060708090A0B0C0D", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP104HexWithKeyIndex) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep",
             "0:0102030405060708090a0b0c0d", e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(WiFiMainTest, GetWifiServiceWEP104HexWithKeyIndexAndBase) {
  Error e;
  GetService("wifi", "an_ssid", "managed", "wep",
             "0:0x0102030405060708090a0b0c0d", e);
  EXPECT_TRUE(e.IsSuccess());
}

}  // namespace shill
