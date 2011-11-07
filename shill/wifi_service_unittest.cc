// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <map>
#include <string>
#include <vector>

#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/mock_wifi.h"
#include "shill/property_store_unittest.h"
#include "shill/wpa_supplicant.h"

using std::map;
using std::string;
using std::vector;

namespace shill {
using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

class WiFiServiceTest : public PropertyStoreTest {
 public:
  WiFiServiceTest() : wifi_(
      new NiceMock<MockWiFi>(
          control_interface(),
          dispatcher(),
          manager(),
          "wifi",
          fake_mac,
          0)) {}
  virtual ~WiFiServiceTest() {}

 protected:
  static const char fake_mac[];
  scoped_refptr<MockWiFi> wifi() { return wifi_; }

 private:
  scoped_refptr<MockWiFi> wifi_;
};

// static
const char WiFiServiceTest::fake_mac[] = "AaBBcCDDeeFF";

class WiFiServiceSecurityTest : public WiFiServiceTest {
 public:
  WiFiServiceRefPtr CreateServiceWithSecurity(const string &security) {
    vector<uint8_t> ssid(5, 0);
    ssid.push_back(0xff);

    return new WiFiService(control_interface(),
                           dispatcher(),
                           manager(),
                           wifi(),
                           ssid,
                           flimflam::kModeManaged,
                           security);
  }

  bool TestStorageSecurityIs(WiFiServiceRefPtr wifi_service,
                             const string &security) {
    string id = wifi_service->GetStorageIdentifier();
    size_t mac_pos = id.find(StringToLowerASCII(string(fake_mac)));
    EXPECT_NE(mac_pos, string::npos);
    size_t mode_pos = id.find(string(flimflam::kModeManaged), mac_pos);
    EXPECT_NE(mode_pos, string::npos);
    return id.find(string(security), mode_pos) != string::npos;
  }

  // Test that a service that is created with security |from_security|
  // gets by default a storage identifier with |to_security| as its
  // security component.
  bool TestStorageMapping(const string &from_security,
                          const string &to_security) {
    WiFiServiceRefPtr wifi_service = CreateServiceWithSecurity(from_security);
    return TestStorageSecurityIs(wifi_service, to_security);
  }

  // Test whether a service of type |service_security| can load from a
  // storage interface containing an entry for |storage_security|.
  // Make sure the result meets |expectation|.  If |expectation| is
  // true, also make sure the service storage identifier changes to
  // match |storage_security|.
  bool TestLoadMapping(const string &service_security,
                       const string &storage_security,
                       bool expectation) {
    WiFiServiceRefPtr wifi_service =
        CreateServiceWithSecurity(service_security);
    NiceMock<MockStore> mock_store;
    const string storage_id =
        wifi_service->GetStorageIdentifierForSecurity(storage_security);
    EXPECT_CALL(mock_store, ContainsGroup(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(mock_store, ContainsGroup(StrEq(storage_id)))
        .WillRepeatedly(Return(true));
    bool is_loadable = wifi_service->IsLoadableFrom(&mock_store);
    EXPECT_EQ(expectation, is_loadable);
    bool is_loaded = wifi_service->Load(&mock_store);
    EXPECT_EQ(expectation, is_loaded);

    if (expectation != is_loadable || expectation != is_loaded) {
      return false;
    } else if (!expectation) {
      return true;
    } else {
      return TestStorageSecurityIs(wifi_service, storage_security);
    }
  }
};

MATCHER(WPASecurityArgs, "") {
  return ContainsKey(arg, wpa_supplicant::kPropertySecurityProtocol) &&
      ContainsKey(arg, wpa_supplicant::kPropertyPreSharedKey);
}

TEST_F(WiFiServiceTest, StorageId) {
  vector<uint8_t> ssid(5, 0);
  ssid.push_back(0xff);

  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityNone);
  string id = wifi_service->GetStorageIdentifier();
  for (uint i = 0; i < id.length(); ++i) {
    EXPECT_TRUE(id[i] == '_' ||
                isxdigit(id[i]) ||
                (isalpha(id[i]) && islower(id[i])));
  }
  EXPECT_TRUE(wifi_service->TechnologyIs(Technology::kWifi));
  size_t mac_pos = id.find(StringToLowerASCII(string(fake_mac)));
  EXPECT_NE(mac_pos, string::npos);
  EXPECT_NE(id.find(string(flimflam::kModeManaged), mac_pos), string::npos);
}

TEST_F(WiFiServiceTest, NonUTF8SSID) {
  vector<uint8_t> ssid;

  ssid.push_back(0xff);  // not a valid UTF-8 byte-sequence
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityNone);
  map<string, ::DBus::Variant> properties;
  // if service doesn't propertly sanitize SSID, this will generate SIGABRT.
  DBusAdaptor::GetProperties(wifi_service->store(), &properties, NULL);
}

TEST_F(WiFiServiceTest, ConnectTaskWPA) {
  vector<uint8_t> ssid(5, 0);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityWpa);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPASecurityArgs()));
  wifi_service->ConnectTask();
}

TEST_F(WiFiServiceTest, ConnectTaskRSN) {
  vector<uint8_t> ssid(5, 0);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityRsn);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPASecurityArgs()));
  wifi_service->ConnectTask();
}

TEST_F(WiFiServiceTest, ConnectTaskPSK) {
  vector<uint8_t> ssid(5, 0);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityPsk);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPASecurityArgs()));
  wifi_service->ConnectTask();
}

TEST_F(WiFiServiceTest, LoadHidden) {
  vector<uint8_t> ssid(5, 0);
  ssid.push_back(0xff);

  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurityNone);
  ASSERT_FALSE(service->hidden_ssid_);
  NiceMock<MockStore> mock_store;
  const string storage_id = service->GetStorageIdentifier();
  EXPECT_CALL(mock_store, ContainsGroup(StrEq(storage_id)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_store, GetBool(_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_store,
              GetBool(StrEq(storage_id), WiFiService::kStorageHiddenSSID, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(true), Return(true)));
  EXPECT_TRUE(service->Load(&mock_store));
  EXPECT_TRUE(service->hidden_ssid_);
}

TEST_F(WiFiServiceSecurityTest, WPAMapping) {
  EXPECT_TRUE(TestStorageMapping(flimflam::kSecurityRsn,
                                 flimflam::kSecurityPsk));
  EXPECT_TRUE(TestStorageMapping(flimflam::kSecurityWpa,
                                 flimflam::kSecurityPsk));
  EXPECT_TRUE(TestStorageMapping(flimflam::kSecurityPsk,
                                 flimflam::kSecurityPsk));
  EXPECT_TRUE(TestStorageMapping(flimflam::kSecurityWep,
                                 flimflam::kSecurityWep));
  EXPECT_TRUE(TestStorageMapping(flimflam::kSecurityNone,
                                 flimflam::kSecurityNone));
  // TODO(pstew): 802.1x is in a NOTIMPLEMENTED block in wifi_service.cc
  // EXPECT_TRUE(TestStorageMapping(flimflam::kSecurity8021x,
  //                                flimflam::kSecurity8021x));
}

TEST_F(WiFiServiceSecurityTest, LoadMapping) {
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityRsn,
                              flimflam::kSecurityPsk,
                              true));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityRsn,
                              flimflam::kSecurityRsn,
                              true));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityRsn,
                              flimflam::kSecurityWpa,
                              false));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityWpa,
                              flimflam::kSecurityPsk,
                              true));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityWpa,
                              flimflam::kSecurityWpa,
                              true));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityWpa,
                              flimflam::kSecurityRsn,
                              false));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityWep,
                              flimflam::kSecurityWep,
                              true));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityWep,
                              flimflam::kSecurityPsk,
                              false));
}

}  // namespace shill
