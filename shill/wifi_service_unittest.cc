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
#include "shill/mock_nss.h"
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
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrNe;

namespace shill {

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

  bool CheckConnectable(const std::string &security, const char *passphrase,
                        Service::EapCredentials *eap) {
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
    if (eap) {
      service->set_eap(*eap);
    }
    return service->connectable();
  }
  WiFiEndpoint *MakeEndpoint(const string &ssid, const string &bssid,
                             uint16 frequency, int16 signal_dbm) {
    return WiFiEndpoint::MakeOpenEndpoint(
        NULL, NULL, ssid, bssid, frequency, signal_dbm);
  }
  WiFiService *MakeGenericService() {
    return new WiFiService(control_interface(),
                           dispatcher(),
                           metrics(),
                           manager(),
                           wifi(),
                           vector<uint8_t>(),
                           flimflam::kModeManaged,
                           flimflam::kSecurityWep,
                           false);
  }
  ServiceMockAdaptor *GetAdaptor(WiFiService *service) {
    return dynamic_cast<ServiceMockAdaptor *>(service->adaptor());
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

class WiFiServiceUpdateFromEndpointsTest : public WiFiServiceTest {
 public:
  WiFiServiceUpdateFromEndpointsTest()
      : kOkEndpointStrength(WiFiService::SignalToStrength(kOkEndpointSignal)),
        kBadEndpointStrength(WiFiService::SignalToStrength(kBadEndpointSignal)),
        kGoodEndpointStrength(
            WiFiService::SignalToStrength(kGoodEndpointSignal)),
        service(MakeGenericService()),
        adaptor(*GetAdaptor(service)) {
    ok_endpoint = MakeEndpoint(
        "", "00:00:00:00:01", kOkEndpointFrequency, kOkEndpointSignal);
    good_endpoint = MakeEndpoint(
        "", "00:00:00:00:02", kGoodEndpointFrequency, kGoodEndpointSignal);
    bad_endpoint = MakeEndpoint(
        "", "00:00:00:00:03", kBadEndpointFrequency, kBadEndpointSignal);
  }

 protected:
  static const uint16 kOkEndpointFrequency = 2422;
  static const uint16 kBadEndpointFrequency = 2417;
  static const uint16 kGoodEndpointFrequency = 2412;
  static const int16 kOkEndpointSignal = -50;
  static const int16 kBadEndpointSignal = -75;
  static const int16 kGoodEndpointSignal = -25;
  // Can't be both static and const (because initialization requires a
  // function call). So choose to be just const.
  const uint8 kOkEndpointStrength;
  const uint8 kBadEndpointStrength;
  const uint8 kGoodEndpointStrength;
  WiFiEndpointRefPtr ok_endpoint;
  WiFiEndpointRefPtr bad_endpoint;
  WiFiEndpointRefPtr good_endpoint;
  WiFiServiceRefPtr service;
  ServiceMockAdaptor &adaptor;
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

MATCHER(EAPSecurityArgs, "") {
  return ContainsKey(arg, wpa_supplicant::kNetworkPropertyEapIdentity) &&
      ContainsKey(arg, wpa_supplicant::kNetworkPropertyCaPath);
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
  Error error;
  wifi_service->SetPassphrase("0:mumblemumblem", &error);
  wifi_service->Connect(NULL);
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
  Error error;
  wifi_service->SetPassphrase("0:mumblemumblem", &error);
  wifi_service->Connect(NULL);
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
  Error error;
  wifi_service->SetPassphrase("0:mumblemumblem", &error);
  wifi_service->Connect(NULL);
}

TEST_F(WiFiServiceTest, ConnectTask8021x) {
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurity8021x,
                                                   false);
  Service::EapCredentials eap;
  eap.identity = "identity";
  eap.password = "mumble";
  wifi_service->set_eap(eap);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), EAPSecurityArgs()));
  wifi_service->Connect(NULL);
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
  wifi_service->Connect(NULL);

  wifi_service->SetPassphrase("abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex0()));
  wifi_service->Connect(NULL);

  wifi_service->SetPassphrase("1:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex1()));
  wifi_service->Connect(NULL);

  wifi_service->SetPassphrase("2:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex2()));
  wifi_service->Connect(NULL);

  wifi_service->SetPassphrase("3:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex3()));
  wifi_service->Connect(NULL);
}


MATCHER(DynamicWEPArgs, "") {
  return ContainsKey(arg, wpa_supplicant::kNetworkPropertyEapIdentity) &&
      ContainsKey(arg, wpa_supplicant::kNetworkPropertyCaPath) &&
      !ContainsKey(arg, wpa_supplicant::kPropertySecurityProtocol);
}

// Dynamic WEP + 802.1x.
TEST_F(WiFiServiceTest, ConnectTaskDynamicWEP) {
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

  Service::EapCredentials eap;
  eap.key_management = "IEEE8021X";
  eap.identity = "something";
  eap.password = "mumble";
  wifi_service->set_eap(eap);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), DynamicWEPArgs()));
  wifi_service->Connect(NULL);
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
  EXPECT_TRUE(TestStorageMapping(flimflam::kSecurity8021x,
                                 flimflam::kSecurity8021x));
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

TEST_F(WiFiServiceTest, UnloadAndClearCacheWep) {
  vector<uint8_t> ssid(1, 'a');
  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurityWep,
                                              false);
  // A WEP network does not incur cached credentials.
  EXPECT_CALL(*wifi(), ClearCachedCredentials()).Times(0);
  service->Unload();
}

TEST_F(WiFiServiceTest, UnloadAndClearCache8021x) {
  vector<uint8_t> ssid(1, 'a');
  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurity8021x,
                                              false);
  // An 802.1x network should clear its cached credentials.
  EXPECT_CALL(*wifi(), ClearCachedCredentials()).Times(1);
  service->Unload();
}

TEST_F(WiFiServiceTest, ParseStorageIdentifierNone) {
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

TEST_F(WiFiServiceTest, ParseStorageIdentifier8021x) {
  // Do a separate test for 802.1x, since kSecurity8021x contains a "_",
  // which needs to be dealt with specially in the parser.
  vector<uint8_t> ssid(5);
  ssid.push_back(0xff);

  WiFiServiceRefPtr service = new WiFiService(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager(),
                                              wifi(),
                                              ssid,
                                              flimflam::kModeManaged,
                                              flimflam::kSecurity8021x,
                                              false);
  const string storage_id = service->GetStorageIdentifier();
  string address;
  string mode;
  string security;
  EXPECT_TRUE(service->ParseStorageIdentifier(storage_id, &address, &mode,
                                              &security));
  EXPECT_EQ(StringToLowerASCII(string(fake_mac)), address);
  EXPECT_EQ(flimflam::kModeManaged, mode);
  EXPECT_EQ(flimflam::kSecurity8021x, security);
}

TEST_F(WiFiServiceTest, Connectable) {
  // Open network should be connectable.
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityNone, NULL, NULL));

  // Open network should remain connectable if we try to set a password on it.
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityNone, "abcde", NULL));

  // WEP network with passphrase set should be connectable.
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityWep, "abcde", NULL));

  // WEP network without passphrase set should NOT be connectable.
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurityWep, NULL, NULL));

  // A bad passphrase should not make a WEP network connectable.
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurityWep, "a", NULL));

  // Similar to WEP, for WPA.
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityWpa, "abcdefgh", NULL));
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurityWpa, NULL, NULL));
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurityWpa, "a", NULL));

  // Unconfigured 802.1x should NOT be connectable.
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurity8021x, NULL, NULL));

  Service::EapCredentials eap;
  // Empty EAP credentials should not make a 802.1x network connectable.
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurity8021x, NULL, &eap));

  eap.identity = "something";
  // If client certificate is being used, a private key must exist.
  eap.client_cert = "some client cert";
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurity8021x, NULL, &eap));
  eap.private_key = "some private key";
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurity8021x, NULL, &eap));

  // Identity is always required.
  eap.identity.clear();
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurity8021x, NULL, &eap));

  eap.identity = "something";
  // For non EAP-TLS types, a password is required.
  eap.eap = "Non-TLS";
  EXPECT_FALSE(CheckConnectable(flimflam::kSecurity8021x, NULL, &eap));
  eap.password = "some password";
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurity8021x, NULL, &eap));
  // Dynamic WEP + 802.1X should be connectable under the same conditions.
  eap.key_management = "IEEE8021X";
  EXPECT_TRUE(CheckConnectable(flimflam::kSecurityWep, NULL, &eap));
}

TEST_F(WiFiServiceTest, IsAutoConnectable) {
  const char *reason;
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
  EXPECT_FALSE(service->IsAutoConnectable(&reason));
  EXPECT_STREQ(WiFiService::kAutoConnNoEndpoint, reason);

  reason = "";
  WiFiEndpointRefPtr endpoint = MakeEndpoint("a", "00:00:00:00:00:01", 0, 0);
  service->AddEndpoint(endpoint);
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service->HasEndpoints());
  EXPECT_TRUE(service->IsAutoConnectable(&reason));
  EXPECT_STREQ("", reason);

  // WiFi only supports connecting to one Service at a time. So, to
  // avoid disrupting connectivity, we only allow auto-connection to
  // a WiFiService when the corresponding WiFi is idle.
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(false));
  EXPECT_TRUE(service->HasEndpoints());
  EXPECT_FALSE(service->IsAutoConnectable(&reason));
  EXPECT_STREQ(WiFiService::kAutoConnBusy, reason);
}

TEST_F(WiFiServiceTest, AutoConnect) {
  const char *reason;
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
  EXPECT_FALSE(service->IsAutoConnectable(&reason));
  EXPECT_CALL(*wifi(), ConnectTo(_, _))
      .Times(0);
  service->AutoConnect();
  dispatcher()->DispatchPendingEvents();

  WiFiEndpointRefPtr endpoint = MakeEndpoint("a", "00:00:00:00:00:01", 0, 0);
  service->AddEndpoint(endpoint);
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service->IsAutoConnectable(&reason));
  EXPECT_CALL(*wifi(), ConnectTo(_, _));
  service->AutoConnect();
  dispatcher()->DispatchPendingEvents();

  Error error;
  service->Disconnect(&error);
  dispatcher()->DispatchPendingEvents();
  EXPECT_FALSE(service->IsAutoConnectable(&reason));
}

TEST_F(WiFiServiceTest, Populate8021x) {
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
  Service::EapCredentials eap;
  eap.identity = "testidentity";
  eap.pin = "xxxx";
  service->set_eap(eap);
  map<string, ::DBus::Variant> params;
  service->Populate8021xProperties(&params);
  // Test that only non-empty 802.1x properties are populated.
  EXPECT_TRUE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEapIdentity));
  EXPECT_FALSE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEapKeyId));
  EXPECT_FALSE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEapCaCert));

  // Test that CA path is set by default.
  EXPECT_TRUE(ContainsKey(params, wpa_supplicant::kNetworkPropertyCaPath));

  // Test that hardware-backed security arguments are not set.
  EXPECT_FALSE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEapPin));
  EXPECT_FALSE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEngine));
  EXPECT_FALSE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEngineId));
}

TEST_F(WiFiServiceTest, Populate8021xNoSystemCAs) {
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
  Service::EapCredentials eap;
  eap.identity = "testidentity";
  eap.use_system_cas = false;
  service->set_eap(eap);
  map<string, ::DBus::Variant> params;
  service->Populate8021xProperties(&params);
  // Test that CA path is not set if use_system_cas is explicitly false.
  EXPECT_FALSE(ContainsKey(params, wpa_supplicant::kNetworkPropertyCaPath));
}

TEST_F(WiFiServiceTest, Populate8021xUsingHardwareAuth) {
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
  Service::EapCredentials eap;
  eap.identity = "testidentity";
  eap.key_id = "key_id";
  eap.pin = "xxxx";
  service->set_eap(eap);
  map<string, ::DBus::Variant> params;
  service->Populate8021xProperties(&params);
  // Test that EAP engine parameters set if key_id is set.
  EXPECT_TRUE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEapPin));
  EXPECT_TRUE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEapKeyId));
  EXPECT_TRUE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEngine));
  EXPECT_TRUE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEngineId));
}

TEST_F(WiFiServiceTest, Populate8021xNSS) {
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
  Service::EapCredentials eap;
  eap.ca_cert_nss = "nss_nickname";
  service->set_eap(eap);
  MockNSS nss;
  service->nss_ = &nss;

  const string kNSSCertfile("/tmp/nss-cert");
  FilePath nss_cert(kNSSCertfile);
  vector<char> ssid_in_chars(ssid.begin(), ssid.end());
  EXPECT_CALL(nss, GetDERCertfile(eap.ca_cert_nss, ssid_in_chars))
      .WillOnce(Return(nss_cert));

  map<string, ::DBus::Variant> params;
  service->Populate8021xProperties(&params);
  EXPECT_TRUE(ContainsKey(params, wpa_supplicant::kNetworkPropertyEapCaCert));
  if (ContainsKey(params, wpa_supplicant::kNetworkPropertyEapCaCert)) {
    EXPECT_EQ(kNSSCertfile, params[wpa_supplicant::kNetworkPropertyEapCaCert]
              .reader().get_string());
  }
}

TEST_F(WiFiServiceTest, ClearWriteOnlyDerivedProperty) {
  vector<uint8_t> ssid(1, 'a');
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityWep,
                                                   false);

  EXPECT_EQ("", wifi_service->passphrase_);

  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::SetProperty(
      wifi_service->mutable_store(),
      flimflam::kPassphraseProperty,
      DBusAdaptor::StringToVariant("0:abcde"),
      &error));
  EXPECT_EQ("0:abcde", wifi_service->passphrase_);

  EXPECT_TRUE(DBusAdaptor::ClearProperty(wifi_service->mutable_store(),
                                         flimflam::kPassphraseProperty,
                                         &error));
  EXPECT_EQ("", wifi_service->passphrase_);
}

TEST_F(WiFiServiceTest, SignalToStrength) {
  // Verify that our mapping is sane, in the sense that it preserves ordering.
  // We break the test into two domains, because we assume that positive
  // values aren't actually in dBm.
  for (int16 i = std::numeric_limits<int16>::min(); i < 0; ++i) {
    int16 current_mapped = WiFiService::SignalToStrength(i);
    int16 next_mapped =  WiFiService::SignalToStrength(i+1);
    EXPECT_LE(current_mapped, next_mapped)
        << "(original values " << i << " " << i+1 << ")";
    EXPECT_GE(current_mapped, Service::kStrengthMin);
    EXPECT_LE(current_mapped, Service::kStrengthMax);
  }
  for (int16 i = 1; i < std::numeric_limits<int16>::max(); ++i) {
    int16 current_mapped = WiFiService::SignalToStrength(i);
    int16 next_mapped =  WiFiService::SignalToStrength(i+1);
    EXPECT_LE(current_mapped, next_mapped)
        << "(original values " << i << " " << i+1 << ")";
    EXPECT_GE(current_mapped, Service::kStrengthMin);
    EXPECT_LE(current_mapped, Service::kStrengthMax);
  }
}

TEST_F(WiFiServiceUpdateFromEndpointsTest, Strengths) {
  // If the chosen signal values don't map to distinct strength
  // values, then we can't expect our other tests to pass. So verify
  // their distinctness.
  EXPECT_TRUE(kOkEndpointStrength != kBadEndpointStrength);
  EXPECT_TRUE(kOkEndpointStrength != kGoodEndpointStrength);
  EXPECT_TRUE(kGoodEndpointStrength != kBadEndpointStrength);
}

TEST_F(WiFiServiceUpdateFromEndpointsTest, Floating) {
  // Initial endpoint updates values.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kOkEndpointFrequency));
  EXPECT_CALL(adaptor,EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kOkEndpointStrength));
  service->AddEndpoint(ok_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Endpoint with stronger signal updates values.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kGoodEndpointFrequency));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kGoodEndpointStrength));
  service->AddEndpoint(good_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Endpoint with lower signal does not change values.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->AddEndpoint(bad_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing non-optimal endpoint does not change values.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->RemoveEndpoint(bad_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing optimal endpoint updates values.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kOkEndpointFrequency));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kOkEndpointStrength));
  service->RemoveEndpoint(good_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing last endpoint updates values (and doesn't crash).
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _));
  EXPECT_CALL(adaptor, EmitUint8Changed(flimflam::kSignalStrengthProperty, _));
  service->RemoveEndpoint(ok_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);
}

TEST_F(WiFiServiceUpdateFromEndpointsTest, Connected) {
  EXPECT_CALL(adaptor, EmitUint16Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitUint8Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitBoolChanged(_, _)).Times(AnyNumber());
  service->AddEndpoint(bad_endpoint);
  service->AddEndpoint(ok_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Setting current endpoint forces adoption of its values, even if it
  // doesn't have the highest signal.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kBadEndpointFrequency));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kBadEndpointStrength));
  service->NotifyCurrentEndpoint(bad_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Adding a better endpoint doesn't matter, when current endpoint is set.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->AddEndpoint(good_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing a better endpoint doesn't matter, when current endpoint is set.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->RemoveEndpoint(good_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing the current endpoint is safe and sane.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kOkEndpointFrequency));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kOkEndpointStrength));
  service->RemoveEndpoint(bad_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Clearing the current endpoint (without removing it) is also safe and sane.
  service->NotifyCurrentEndpoint(ok_endpoint);
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->NotifyCurrentEndpoint(NULL);
  Mock::VerifyAndClearExpectations(&adaptor);
}

TEST_F(WiFiServiceUpdateFromEndpointsTest, EndpointModified) {
  EXPECT_CALL(adaptor, EmitUint16Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitUint8Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitBoolChanged(_, _)).Times(AnyNumber());
  service->AddEndpoint(ok_endpoint);
  service->AddEndpoint(good_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Updating sub-optimal Endpoint doesn't update Service.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  ok_endpoint->signal_strength_ = (kOkEndpointSignal + kGoodEndpointSignal) / 2;
  service->NotifyEndpointUpdated(*ok_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Updating optimal Endpoint updates appropriate Service property.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor, EmitUint8Changed(flimflam::kSignalStrengthProperty, _));
  good_endpoint->signal_strength_ = kGoodEndpointSignal + 1;
  service->NotifyEndpointUpdated(*good_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Change in optimal Endpoint updates Service properties.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kOkEndpointFrequency));
  EXPECT_CALL(adaptor, EmitUint8Changed(flimflam::kSignalStrengthProperty, _));
  ok_endpoint->signal_strength_ = kGoodEndpointSignal + 2;
  service->NotifyEndpointUpdated(*ok_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);
}

}  // namespace shill
