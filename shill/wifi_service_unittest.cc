// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
#include "shill/refptr_types.h"
#include "shill/wifi_endpoint.h"
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
using ::testing::StrNe;

class WiFiServiceTest : public PropertyStoreTest {
 public:
  WiFiServiceTest() : wifi_(
      new NiceMock<MockWiFi>(
          control_interface(),
          dispatcher(),
          metrics(),
          manager(),
          "wifi",
          fake_mac,
          0)) {}
  virtual ~WiFiServiceTest() {}

 protected:
  static const char fake_mac[];
  bool CheckConnectable(const std::string &security, const char *passphrase) {
    Error error;
    vector<uint8_t> ssid(1, 'a');
    WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                                dispatcher(),
                                                metrics(),
                                                manager(),
                                                wifi(),
                                                ssid,
                                                flimflam::kModeManaged,
                                                security,
                                                false);
    if (passphrase)
      service->SetPassphrase(passphrase, &error);
    return service->connectable();
  }
  WiFiEndpoint *MakeEndpoint(const string &ssid, const string &bssid) {
    return WiFiEndpoint::MakeOpenEndpoint(ssid, bssid);
  }
  scoped_refptr<MockWiFi> wifi() { return wifi_; }

 private:
  scoped_refptr<MockWiFi> wifi_;
};

// static
const char WiFiServiceTest::fake_mac[] = "AaBBcCDDeeFF";

class WiFiServiceSecurityTest : public WiFiServiceTest {
 public:
  WiFiServiceRefPtr CreateServiceWithSecurity(const string &security) {
    vector<uint8_t> ssid(5);
    ssid.push_back(0xff);

    return new WiFiService(control_interface(),
                           dispatcher(),
                           metrics(),
                           manager(),
                           wifi(),
                           ssid,
                           flimflam::kModeManaged,
                           security,
                           false);
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

TEST_F(WiFiServiceTest, StorageId) {
  vector<uint8_t> ssid(5);
  ssid.push_back(0xff);

  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityNone,
                                                   false);
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

// Make sure the passphrase is registered as a write only property
// by reading and comparing all string properties returned on the store.
TEST_F(WiFiServiceTest, PassphraseWriteOnly) {
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityWpa,
                                                   false);
  ReadablePropertyConstIterator<string> it =
      (wifi_service->store()).GetStringPropertiesIter();
  for( ; !it.AtEnd(); it.Advance())
    EXPECT_NE(it.Key(), flimflam::kPassphraseProperty);
}

// Make sure setting the passphrase via D-Bus Service.SetProperty validates
// the passphrase.
TEST_F(WiFiServiceTest, PassphraseSetPropertyValidation) {
  // We only spot check two password cases here to make sure the
  // SetProperty code path does validation.  We're not going to exhaustively
  // test for all types of passwords.
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityWep,
                                                   false);
  Error error;
  EXPECT_TRUE(wifi_service->mutable_store()->SetStringProperty(
                  flimflam::kPassphraseProperty, "0:abcde", &error));
  EXPECT_FALSE(wifi_service->mutable_store()->SetStringProperty(
                   flimflam::kPassphraseProperty, "invalid", &error));
  EXPECT_EQ(Error::kInvalidPassphrase, error.type());
}

TEST_F(WiFiServiceTest, PassphraseSetPropertyOpenNetwork) {
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityNone,
                                                   false);
  Error error;
  EXPECT_FALSE(wifi_service->mutable_store()->SetStringProperty(
                   flimflam::kPassphraseProperty, "invalid", &error));
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(WiFiServiceTest, NonUTF8SSID) {
  vector<uint8_t> ssid;

  ssid.push_back(0xff);  // not a valid UTF-8 byte-sequence
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityNone,
                                                   false);
  map<string, ::DBus::Variant> properties;
  // if service doesn't propertly sanitize SSID, this will generate SIGABRT.
  DBusAdaptor::GetProperties(wifi_service->store(), &properties, NULL);
}

MATCHER(WPASecurityArgs, "") {
  return ContainsKey(arg, wpa_supplicant::kPropertySecurityProtocol) &&
      ContainsKey(arg, wpa_supplicant::kPropertyPreSharedKey);
}

TEST_F(WiFiServiceTest, ConnectTaskWPA) {
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityWpa,
                                                   false);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPASecurityArgs()));
  wifi_service->ConnectTask();
}

TEST_F(WiFiServiceTest, ConnectTaskRSN) {
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityRsn,
                                                   false);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPASecurityArgs()));
  wifi_service->ConnectTask();
}

TEST_F(WiFiServiceTest, ConnectTaskPSK) {
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityPsk,
                                                   false);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPASecurityArgs()));
  wifi_service->ConnectTask();
}

MATCHER(WEPSecurityArgsKeyIndex0, "") {
  return ContainsKey(arg, wpa_supplicant::kPropertyAuthAlg) &&
      ContainsKey(arg, wpa_supplicant::kPropertyWEPKey + std::string("0")) &&
      ContainsKey(arg, wpa_supplicant::kPropertyWEPTxKeyIndex) &&
      (arg.find(wpa_supplicant::kPropertyWEPTxKeyIndex)->second.
           reader().get_uint32() == 0);
}

MATCHER(WEPSecurityArgsKeyIndex1, "") {
  return ContainsKey(arg, wpa_supplicant::kPropertyAuthAlg) &&
      ContainsKey(arg, wpa_supplicant::kPropertyWEPKey + std::string("1")) &&
      ContainsKey(arg, wpa_supplicant::kPropertyWEPTxKeyIndex) &&
      (arg.find(wpa_supplicant::kPropertyWEPTxKeyIndex)->second.
           reader().get_uint32() == 1);
}

MATCHER(WEPSecurityArgsKeyIndex2, "") {
  return ContainsKey(arg, wpa_supplicant::kPropertyAuthAlg) &&
      ContainsKey(arg, wpa_supplicant::kPropertyWEPKey + std::string("2")) &&
      ContainsKey(arg, wpa_supplicant::kPropertyWEPTxKeyIndex) &&
      (arg.find(wpa_supplicant::kPropertyWEPTxKeyIndex)->second.
           reader().get_uint32() == 2);
}

MATCHER(WEPSecurityArgsKeyIndex3, "") {
  return ContainsKey(arg, wpa_supplicant::kPropertyAuthAlg) &&
      ContainsKey(arg, wpa_supplicant::kPropertyWEPKey + std::string("3")) &&
      ContainsKey(arg, wpa_supplicant::kPropertyWEPTxKeyIndex) &&
      (arg.find(wpa_supplicant::kPropertyWEPTxKeyIndex)->second.
           reader().get_uint32() == 3);
}

TEST_F(WiFiServiceTest, ConnectTaskWEP) {
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityWep,
                                                   false);
  Error error;
  wifi_service->SetPassphrase("0:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex0()));
  wifi_service->ConnectTask();

  wifi_service->SetPassphrase("abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex0()));
  wifi_service->ConnectTask();

  wifi_service->SetPassphrase("1:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex1()));
  wifi_service->ConnectTask();

  wifi_service->SetPassphrase("2:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex2()));
  wifi_service->ConnectTask();

  wifi_service->SetPassphrase("3:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex3()));
  wifi_service->ConnectTask();
}

TEST_F(WiFiServiceTest, LoadHidden) {
  vector<uint8_t> ssid(5);
  ssid.push_back(0xff);

  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurityNone,
                                              false);
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

TEST_F(WiFiServiceTest, LoadAndUnloadPassphrase) {
  vector<uint8_t> ssid(5);
  ssid.push_back(0xff);

  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurityPsk,
                                              false);
  NiceMock<MockStore> mock_store;
  const string storage_id = service->GetStorageIdentifier();
  EXPECT_CALL(mock_store, ContainsGroup(StrEq(storage_id)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_store, GetBool(_, _, _))
      .WillRepeatedly(Return(false));
  const string passphrase = "passphrase";
  EXPECT_CALL(mock_store,
              GetCryptedString(StrEq(storage_id),
                               WiFiService::kStoragePassphrase, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(passphrase), Return(true)));
  EXPECT_CALL(mock_store,
              GetCryptedString(StrEq(storage_id),
                               StrNe(WiFiService::kStoragePassphrase), _))
      .WillRepeatedly(Return(false));
  EXPECT_TRUE(service->need_passphrase_);
  EXPECT_TRUE(service->Load(&mock_store));
  EXPECT_EQ(passphrase, service->passphrase_);
  EXPECT_TRUE(service->connectable());
  EXPECT_FALSE(service->need_passphrase_);
  service->Unload();
  EXPECT_EQ(string(""), service->passphrase_);
  EXPECT_FALSE(service->connectable());
  EXPECT_TRUE(service->need_passphrase_);
}

TEST_F(WiFiServiceTest, ParseStorageIdentifier) {
  vector<uint8_t> ssid(5);
  ssid.push_back(0xff);

  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurityNone,
                                              false);
  const string storage_id = service->GetStorageIdentifier();
  string address;
  string mode;
  string security;
  EXPECT_TRUE(service->ParseStorageIdentifier(storage_id, &address, &mode,
                                              &security));
  EXPECT_EQ(StringToLowerASCII(string(fake_mac)), address);
  EXPECT_EQ(flimflam::kModeManaged, mode);
  EXPECT_EQ(flimflam::kSecurityNone, security);
}

TEST_F(WiFiServiceTest, Connectable) {
  // Open network should be connectable.
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityNone, NULL));

  // Open network should remain connectable if we try to set a password on it.
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityNone, "abcde"));

  // WEP network with passphrase set should be connectable.
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityWep, "abcde"));

  // WEP network without passphrase set should NOT be connectable.
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurityWep, NULL));

  // A bad passphrase should not make a WEP network connectable.
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurityWep, "a"));

  // Similar to WEP, for WPA.
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityWpa, "abcdefgh"));
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurityWpa, NULL));
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurityWpa, "a"));

  // Unconfigured 802.1x should NOT be connectable.
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurity8021x, NULL));
}

TEST_F(WiFiServiceTest, IsAutoConnectable) {
  vector<uint8_t> ssid(1, 'a');
  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurityNone,
                                              false);
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(service->HasEndpoints());
  EXPECT_FALSE(service->IsAutoConnectable());

  WiFiEndpointRefPtr endpoint = MakeEndpoint("a", "00:00:00:00:00:01");
  service->AddEndpoint(endpoint);
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service->HasEndpoints());
  EXPECT_TRUE(service->IsAutoConnectable());

  // WiFi only supports connecting to one Service at a time. So, to
  // avoid disrupting connectivity, we only allow auto-connection to
  // a WiFiService when the corresponding WiFi is idle.
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(false));
  EXPECT_TRUE(service->HasEndpoints());
  EXPECT_FALSE(service->IsAutoConnectable());
}

TEST_F(WiFiServiceTest, AutoConnect) {
  vector<uint8_t> ssid(1, 'a');
  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurityNone,
                                              false);
  EXPECT_FALSE(service->IsAutoConnectable());
  EXPECT_CALL(*wifi(), ConnectTo(_, _))
      .Times(0);
  service->AutoConnect();

  WiFiEndpointRefPtr endpoint = MakeEndpoint("a", "00:00:00:00:00:01");
  service->AddEndpoint(endpoint);
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service->IsAutoConnectable());
  EXPECT_CALL(*wifi(), ConnectTo(_, _));
  service->AutoConnect();

  Error error;
  service->Disconnect(&error);
  EXPECT_FALSE(service->IsAutoConnectable());
}

}  // namespace shill
