// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/stringprintf.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_certificate_file.h"
#include "shill/mock_control.h"
#include "shill/mock_log.h"
#include "shill/mock_nss.h"
#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/mock_wifi.h"
#include "shill/mock_wifi_provider.h"
#include "shill/property_store_unittest.h"
#include "shill/refptr_types.h"
#include "shill/wifi_endpoint.h"
#include "shill/wpa_supplicant.h"

using base::FilePath;
using std::map;
using std::set;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrNe;
using ::testing::StrictMock;

namespace shill {

class WiFiServiceTest : public PropertyStoreTest {
 public:
  WiFiServiceTest()
       : wifi_(new NiceMock<MockWiFi>(
               control_interface(),
               dispatcher(),
               metrics(),
               manager(),
               "wifi",
               fake_mac,
               0)),
         simple_ssid_(1, 'a'),
         simple_ssid_string_("a") {}
  virtual ~WiFiServiceTest() {}

 protected:
  static const char fake_mac[];

  bool CheckConnectable(const std::string &security, const char *passphrase,
                        EapCredentials *eap) {
    Error error;
    WiFiServiceRefPtr service = MakeSimpleService(security);
    if (passphrase)
      service->SetPassphrase(passphrase, &error);
    if (eap) {
      service->set_eap(*eap);
    }
    return service->connectable();
  }
  WiFiEndpoint *MakeEndpoint(const string &ssid, const string &bssid,
                             uint16 frequency, int16 signal_dbm,
                             bool has_wpa_property, bool has_rsn_property) {
    return WiFiEndpoint::MakeEndpoint(
        NULL, wifi(), ssid, bssid, WPASupplicant::kNetworkModeInfrastructure,
        frequency, signal_dbm, has_wpa_property, has_rsn_property);
  }
  WiFiEndpoint *MakeOpenEndpoint(const string &ssid, const string &bssid,
                                 uint16 frequency, int16 signal_dbm) {
    return WiFiEndpoint::MakeOpenEndpoint(
        NULL, wifi(), ssid, bssid, WPASupplicant::kNetworkModeInfrastructure,
        frequency, signal_dbm);
  }
  WiFiServiceRefPtr MakeSimpleService(const string &security) {
    return new WiFiService(control_interface(),
                           dispatcher(),
                           metrics(),
                           manager(),
                           &provider_,
                           simple_ssid_,
                           flimflam::kModeManaged,
                           security,
                           false);
  }
  WiFiServiceRefPtr MakeGenericService() {
    return MakeSimpleService(flimflam::kSecurityWep);
  }
  void SetWiFiForService(WiFiServiceRefPtr service, WiFiRefPtr wifi) {
    service->wifi_ = wifi;
  }
  WiFiServiceRefPtr MakeServiceWithWiFi(const string &security) {
    WiFiServiceRefPtr service = MakeSimpleService(security);
    SetWiFiForService(service, wifi_);
    return service;
  }
  ServiceMockAdaptor *GetAdaptor(WiFiService *service) {
    return dynamic_cast<ServiceMockAdaptor *>(service->adaptor());
  }
  Error::Type TestConfigurePassphrase(const string &security,
                                      const char *passphrase) {
    WiFiServiceRefPtr service = MakeSimpleService(security);
    KeyValueStore args;
    if (passphrase) {
      args.SetString(flimflam::kPassphraseProperty, passphrase);
    }
    Error error;
    service->Configure(args, &error);
    return error.type();
  }
  scoped_refptr<MockWiFi> wifi() { return wifi_; }
  MockWiFiProvider *provider() { return &provider_; }
  string GetAnyDeviceAddress() { return WiFiService::kAnyDeviceAddress; }
  const vector<uint8_t> &simple_ssid() { return simple_ssid_; }
  const string &simple_ssid_string() { return simple_ssid_string_; }

 private:
  scoped_refptr<MockWiFi> wifi_;
  MockWiFiProvider provider_;
  const vector<uint8_t> simple_ssid_;
  const string simple_ssid_string_;
};

// static
const char WiFiServiceTest::fake_mac[] = "AaBBcCDDeeFF";

MATCHER_P3(ContainsWiFiProperties, ssid, mode, security, "") {
  string hex_ssid = base::HexEncode(ssid.data(), ssid.size());
  return
      arg.ContainsString(WiFiService::kStorageType) &&
      arg.GetString(WiFiService::kStorageType) == flimflam::kTypeWifi &&
      arg.ContainsString(WiFiService::kStorageSSID) &&
      arg.GetString(WiFiService::kStorageSSID) == hex_ssid &&
      arg.ContainsString(WiFiService::kStorageMode) &&
      arg.GetString(WiFiService::kStorageMode) == mode &&
      arg.ContainsString(WiFiService::kStorageSecurityClass) &&
      arg.GetString(WiFiService::kStorageSecurityClass) == security;
}

class WiFiServiceSecurityTest : public WiFiServiceTest {
 public:
  bool TestStorageSecurityIs(WiFiServiceRefPtr wifi_service,
                             const string &security) {
    string id = wifi_service->GetStorageIdentifier();
    size_t mac_pos = id.find(StringToLowerASCII(GetAnyDeviceAddress()));
    EXPECT_NE(mac_pos, string::npos);
    size_t mode_pos = id.find(string(flimflam::kModeManaged), mac_pos);
    EXPECT_NE(mode_pos, string::npos);
    return id.find(string(security), mode_pos) != string::npos;
  }

  // Test that a service that is created with security |from_security|
  // gets by default a storage identifier with |to_security| as its
  // security component, and that when saved, it sets the Security
  // property in to |to_security| as well.
  bool TestStorageMapping(const string &from_security,
                          const string &to_security) {
    WiFiServiceRefPtr wifi_service = MakeSimpleService(from_security);
    NiceMock<MockStore> mock_store;
    EXPECT_CALL(mock_store, SetString(_, _, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_store,
                SetString(_, WiFiService::kStorageSecurity, from_security))
        .Times(1);
    EXPECT_CALL(mock_store,
                SetString(_, WiFiService::kStorageSecurityClass, to_security))
        .Times(1);
    wifi_service->Save(&mock_store);
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
    WiFiServiceRefPtr wifi_service = MakeSimpleService(service_security);
    NiceMock<MockStore> mock_store;
    EXPECT_CALL(mock_store, GetGroupsWithProperties(_))
        .WillRepeatedly(Return(set<string>()));
    const string kStorageId = "storage_id";
    EXPECT_CALL(mock_store, ContainsGroup(kStorageId))
        .WillRepeatedly(Return(true));
    set<string> groups;
    groups.insert(kStorageId);
    EXPECT_CALL(mock_store, GetGroupsWithProperties(
        ContainsWiFiProperties(wifi_service->ssid(),
                               flimflam::kModeManaged,
                               storage_security)))
        .WillRepeatedly(Return(groups));
    bool is_loadable = wifi_service->IsLoadableFrom(&mock_store);
    EXPECT_EQ(expectation, is_loadable);
    bool is_loaded = wifi_service->Load(&mock_store);
    EXPECT_EQ(expectation, is_loaded);

    if (expectation != is_loadable || expectation != is_loaded) {
      return false;
    } else if (!expectation) {
      return true;
    } else {
      return wifi_service->GetStorageIdentifier() == kStorageId;
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
    ok_endpoint = MakeOpenEndpoint(
        simple_ssid_string(), kOkEndpointBssId, kOkEndpointFrequency,
        kOkEndpointSignal);
    good_endpoint = MakeOpenEndpoint(
        simple_ssid_string(), kGoodEndpointBssId, kGoodEndpointFrequency,
        kGoodEndpointSignal);
    bad_endpoint = MakeOpenEndpoint(
        simple_ssid_string(), kBadEndpointBssId, kBadEndpointFrequency,
        kBadEndpointSignal);
  }

 protected:
  static const uint16 kOkEndpointFrequency = 2422;
  static const uint16 kBadEndpointFrequency = 2417;
  static const uint16 kGoodEndpointFrequency = 2412;
  static const int16 kOkEndpointSignal = -50;
  static const int16 kBadEndpointSignal = -75;
  static const int16 kGoodEndpointSignal = -25;
  static const char *kOkEndpointBssId;
  static const char *kGoodEndpointBssId;
  static const char *kBadEndpointBssId;
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

const char *WiFiServiceUpdateFromEndpointsTest::kOkEndpointBssId =
    "00:00:00:00:00:01";
const char *WiFiServiceUpdateFromEndpointsTest::kGoodEndpointBssId =
    "00:00:00:00:00:02";
const char *WiFiServiceUpdateFromEndpointsTest::kBadEndpointBssId =
    "00:00:00:00:00:03";

class WiFiServiceFixupStorageTest : public WiFiServiceTest {
 protected:
  void AddGroup(string group_name) {
    groups_.insert(group_name);
  }

  void AddServiceEntry(bool has_type, bool has_mode, bool has_security,
                       bool has_security_class) {
    int index = groups_.size();
    string id = base::StringPrintf("%s_%d_%d_%s_%s", flimflam::kTypeWifi,
                                   index, index, flimflam::kModeManaged,
                                   flimflam::kSecurityWpa);
    AddGroup(id);
    EXPECT_CALL(store_, GetString(id, WiFiService::kStorageType, _))
        .WillOnce(Return(has_type));
    if (!has_type) {
      EXPECT_CALL(store_, SetString(id, WiFiService::kStorageType,
                                    flimflam::kTypeWifi));
    }
    EXPECT_CALL(store_, GetString(id, WiFiService::kStorageMode, _))
        .WillOnce(Return(has_mode));
    if (!has_mode) {
      EXPECT_CALL(store_, SetString(id, WiFiService::kStorageMode,
                                    flimflam::kModeManaged));
    }
    EXPECT_CALL(store_, GetString(id, WiFiService::kStorageSecurity, _))
        .WillOnce(Return(has_security));
    if (!has_security) {
      EXPECT_CALL(store_, SetString(id, WiFiService::kStorageSecurity,
                                    flimflam::kSecurityWpa));
    }
    EXPECT_CALL(store_, GetString(id, WiFiService::kStorageSecurityClass, _))
        .WillOnce(Return(has_security_class));
    if (!has_security_class) {
      EXPECT_CALL(store_, SetString(id, WiFiService::kStorageSecurityClass,
                                    flimflam::kSecurityPsk));
    }
  }

  bool FixupServiceEntries() {
    EXPECT_CALL(store_, GetGroups()).WillOnce(Return(groups_));
    return WiFiService::FixupServiceEntries(&store_);
  }

 private:
  StrictMock<MockStore> store_;
  set<string> groups_;
};

TEST_F(WiFiServiceTest, StorageId) {
  WiFiServiceRefPtr wifi_service = MakeSimpleService(flimflam::kSecurityNone);
  string id = wifi_service->GetStorageIdentifier();
  for (uint i = 0; i < id.length(); ++i) {
    EXPECT_TRUE(id[i] == '_' ||
                isxdigit(id[i]) ||
                (isalpha(id[i]) && islower(id[i])));
  }
  size_t mac_pos = id.find(StringToLowerASCII(GetAnyDeviceAddress()));
  EXPECT_NE(mac_pos, string::npos);
  EXPECT_NE(id.find(string(flimflam::kModeManaged), mac_pos), string::npos);
}

// Make sure the passphrase is registered as a write only property
// by reading and comparing all string properties returned on the store.
TEST_F(WiFiServiceTest, PassphraseWriteOnly) {
  WiFiServiceRefPtr wifi_service = MakeSimpleService(flimflam::kSecurityWpa);
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
  WiFiServiceRefPtr wifi_service = MakeSimpleService(flimflam::kSecurityWep);
  Error error;
  EXPECT_TRUE(wifi_service->mutable_store()->SetStringProperty(
                  flimflam::kPassphraseProperty, "0:abcde", &error));
  EXPECT_FALSE(wifi_service->mutable_store()->SetStringProperty(
                   flimflam::kPassphraseProperty, "invalid", &error));
  EXPECT_EQ(Error::kInvalidPassphrase, error.type());
}

TEST_F(WiFiServiceTest, PassphraseSetPropertyOpenNetwork) {
  WiFiServiceRefPtr wifi_service = MakeSimpleService(flimflam::kSecurityNone);
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
                                                   provider(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityNone,
                                                   false);
  map<string, ::DBus::Variant> properties;
  // if service doesn't propertly sanitize SSID, this will generate SIGABRT.
  DBusAdaptor::GetProperties(wifi_service->store(), &properties, NULL);
}

MATCHER(PSKSecurityArgs, "") {
  return ContainsKey(arg, WPASupplicant::kPropertySecurityProtocol) &&
      arg.find(WPASupplicant::kPropertySecurityProtocol)->second.
           reader().get_string() == string("WPA RSN") &&
      ContainsKey(arg, WPASupplicant::kPropertyPreSharedKey);
}

MATCHER(WPA80211wSecurityArgs, "") {
  return ContainsKey(arg, WPASupplicant::kPropertySecurityProtocol) &&
      ContainsKey(arg, WPASupplicant::kPropertyPreSharedKey) &&
      ContainsKey(arg, WPASupplicant::kNetworkPropertyIeee80211w);
}

MATCHER(EAPSecurityArgs, "") {
  return ContainsKey(arg, WPASupplicant::kNetworkPropertyEapIdentity) &&
      ContainsKey(arg, WPASupplicant::kNetworkPropertyCaPath);
}

MATCHER_P(FrequencyArg, has_arg, "") {
  return has_arg ==
      ContainsKey(arg, WPASupplicant::kNetworkPropertyFrequency);
}

TEST_F(WiFiServiceTest, ConnectTaskWPA) {
  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityWpa);
  EXPECT_CALL(*wifi(), ConnectTo(wifi_service.get(), PSKSecurityArgs()));
  Error error;
  wifi_service->SetPassphrase("0:mumblemumblem", &error);
  wifi_service->Connect(NULL, "in test");
}

TEST_F(WiFiServiceTest, ConnectTaskRSN) {
  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityRsn);
  EXPECT_CALL(*wifi(), ConnectTo(wifi_service.get(), PSKSecurityArgs()));
  Error error;
  wifi_service->SetPassphrase("0:mumblemumblem", &error);
  wifi_service->Connect(NULL, "in test");
}

TEST_F(WiFiServiceTest, ConnectConditions) {
  Error error;
  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityNone);
  scoped_refptr<MockProfile> mock_profile(
      new NiceMock<MockProfile>(control_interface(), metrics(), manager()));
  wifi_service->set_profile(mock_profile);
  // With nothing else going on, the service should attempt to connect.
  EXPECT_CALL(*wifi(), ConnectTo(wifi_service.get(), _));
  wifi_service->Connect(&error, "in test");
  Mock::VerifyAndClearExpectations(wifi());

  // But if we're already "connecting" or "connected" then we shouldn't attempt
  // again.
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), _)).Times(0);
  wifi_service->SetState(Service::kStateAssociating);
  wifi_service->Connect(&error, "in test");
  wifi_service->SetState(Service::kStateConfiguring);
  wifi_service->Connect(&error, "in test");
  wifi_service->SetState(Service::kStateConnected);
  wifi_service->Connect(&error, "in test");
  wifi_service->SetState(Service::kStatePortal);
  wifi_service->Connect(&error, "in test");
  wifi_service->SetState(Service::kStateOnline);
  wifi_service->Connect(&error, "in test");
  Mock::VerifyAndClearExpectations(wifi());
}

TEST_F(WiFiServiceTest, ConnectTaskPSK) {
  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityPsk);
  EXPECT_CALL(*wifi(), ConnectTo(wifi_service.get(), PSKSecurityArgs()));
  Error error;
  wifi_service->SetPassphrase("0:mumblemumblem", &error);
  wifi_service->Connect(NULL, "in test");
}

TEST_F(WiFiServiceTest, ConnectTask8021x) {
  WiFiServiceRefPtr service = MakeServiceWithWiFi(flimflam::kSecurity8021x);
  EapCredentials eap;
  eap.identity = "identity";
  eap.password = "mumble";
  service->set_eap(eap);
  EXPECT_CALL(*wifi(), ConnectTo(service.get(), EAPSecurityArgs()));
  service->Connect(NULL, "in test");
}

TEST_F(WiFiServiceTest, ConnectTaskAdHocFrequency) {
  vector<uint8_t> ssid(1, 'a');
  WiFiEndpointRefPtr endpoint_nofreq =
      MakeOpenEndpoint("a", "00:00:00:00:00:01", 0, 0);
  WiFiEndpointRefPtr endpoint_freq =
      MakeOpenEndpoint("a", "00:00:00:00:00:02", 2412, 0);

  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityNone);
  wifi_service->AddEndpoint(endpoint_freq);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), FrequencyArg(false)));
  wifi_service->Connect(NULL, "in test");

  wifi_service = new WiFiService(control_interface(),
                                 dispatcher(),
                                 metrics(),
                                 manager(),
                                 provider(),
                                 ssid,
                                 flimflam::kModeAdhoc,
                                 flimflam::kSecurityNone,
                                 false);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), FrequencyArg(false)));
  SetWiFiForService(wifi_service, wifi());
  wifi_service->Connect(NULL, "in test");

  wifi_service = new WiFiService(control_interface(),
                                 dispatcher(),
                                 metrics(),
                                 manager(),
                                 provider(),
                                 ssid,
                                 flimflam::kModeAdhoc,
                                 flimflam::kSecurityNone,
                                 false);
  wifi_service->AddEndpoint(endpoint_nofreq);
  SetWiFiForService(wifi_service, wifi());
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), FrequencyArg(false)));
  wifi_service->Connect(NULL, "in test");

  wifi_service = new WiFiService(control_interface(),
                                 dispatcher(),
                                 metrics(),
                                 manager(),
                                 provider(),
                                 ssid,
                                 flimflam::kModeAdhoc,
                                 flimflam::kSecurityNone,
                                 false);
  wifi_service->AddEndpoint(endpoint_freq);
  SetWiFiForService(wifi_service, wifi());
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), FrequencyArg(true)));
  wifi_service->Connect(NULL, "in test");
}

TEST_F(WiFiServiceTest, ConnectTaskWPA80211w) {
  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityPsk);
  WiFiEndpointRefPtr endpoint =
      MakeOpenEndpoint("a", "00:00:00:00:00:01", 0, 0);
  endpoint->ieee80211w_required_ = true;
  wifi_service->AddEndpoint(endpoint);
  Error error;
  wifi_service->SetPassphrase("0:mumblemumblem", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPA80211wSecurityArgs()));
  wifi_service->Connect(NULL, "in test");
}

MATCHER(WEPSecurityArgsKeyIndex0, "") {
  return ContainsKey(arg, WPASupplicant::kPropertyAuthAlg) &&
      ContainsKey(arg, WPASupplicant::kPropertyWEPKey + std::string("0")) &&
      ContainsKey(arg, WPASupplicant::kPropertyWEPTxKeyIndex) &&
      (arg.find(WPASupplicant::kPropertyWEPTxKeyIndex)->second.
           reader().get_uint32() == 0);
}

MATCHER(WEPSecurityArgsKeyIndex1, "") {
  return ContainsKey(arg, WPASupplicant::kPropertyAuthAlg) &&
      ContainsKey(arg, WPASupplicant::kPropertyWEPKey + std::string("1")) &&
      ContainsKey(arg, WPASupplicant::kPropertyWEPTxKeyIndex) &&
      (arg.find(WPASupplicant::kPropertyWEPTxKeyIndex)->second.
           reader().get_uint32() == 1);
}

MATCHER(WEPSecurityArgsKeyIndex2, "") {
  return ContainsKey(arg, WPASupplicant::kPropertyAuthAlg) &&
      ContainsKey(arg, WPASupplicant::kPropertyWEPKey + std::string("2")) &&
      ContainsKey(arg, WPASupplicant::kPropertyWEPTxKeyIndex) &&
      (arg.find(WPASupplicant::kPropertyWEPTxKeyIndex)->second.
           reader().get_uint32() == 2);
}

MATCHER(WEPSecurityArgsKeyIndex3, "") {
  return ContainsKey(arg, WPASupplicant::kPropertyAuthAlg) &&
      ContainsKey(arg, WPASupplicant::kPropertyWEPKey + std::string("3")) &&
      ContainsKey(arg, WPASupplicant::kPropertyWEPTxKeyIndex) &&
      (arg.find(WPASupplicant::kPropertyWEPTxKeyIndex)->second.
           reader().get_uint32() == 3);
}

TEST_F(WiFiServiceTest, ConnectTaskWEP) {
  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityWep);
  Error error;
  wifi_service->SetPassphrase("0:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex0()));
  wifi_service->Connect(NULL, "in test");

  wifi_service->SetPassphrase("abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex0()));
  wifi_service->Connect(NULL, "in test");

  wifi_service->SetPassphrase("1:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex1()));
  wifi_service->Connect(NULL, "in test");

  wifi_service->SetPassphrase("2:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex2()));
  wifi_service->Connect(NULL, "in test");

  wifi_service->SetPassphrase("3:abcdefghijklm", &error);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WEPSecurityArgsKeyIndex3()));
  wifi_service->Connect(NULL, "in test");
}


MATCHER(DynamicWEPArgs, "") {
  return ContainsKey(arg, WPASupplicant::kNetworkPropertyEapIdentity) &&
      ContainsKey(arg, WPASupplicant::kNetworkPropertyCaPath) &&
      !ContainsKey(arg, WPASupplicant::kPropertySecurityProtocol);
}

// Dynamic WEP + 802.1x.
TEST_F(WiFiServiceTest, ConnectTaskDynamicWEP) {
  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityWep);

  EapCredentials eap;
  eap.key_management = "IEEE8021X";
  eap.identity = "something";
  eap.password = "mumble";
  wifi_service->set_eap(eap);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), DynamicWEPArgs()));
  wifi_service->Connect(NULL, "in test");
}

TEST_F(WiFiServiceTest, SetPassphraseRemovesCachedCredentials) {
  vector<uint8_t> ssid(5);
  WiFiServiceRefPtr wifi_service = MakeServiceWithWiFi(flimflam::kSecurityRsn);

  const string kPassphrase = "abcdefgh";

  {
    Error error;
    // A changed passphrase should trigger cache removal.
    EXPECT_CALL(*wifi(), ClearCachedCredentials(wifi_service.get()));
    wifi_service->SetPassphrase(kPassphrase, &error);
    Mock::VerifyAndClearExpectations(wifi());
    EXPECT_TRUE(error.IsSuccess());
  }

  {
    Error error;
    // An unchanged passphrase should not trigger cache removal.
    EXPECT_CALL(*wifi(), ClearCachedCredentials(_)).Times(0);
    wifi_service->SetPassphrase(kPassphrase, &error);
    Mock::VerifyAndClearExpectations(wifi());
    EXPECT_TRUE(error.IsSuccess());
  }

  {
    Error error;
    // A modified passphrase should trigger cache removal.
    EXPECT_CALL(*wifi(), ClearCachedCredentials(wifi_service.get()));
    wifi_service->SetPassphrase(kPassphrase + "X", &error);
    Mock::VerifyAndClearExpectations(wifi());
    EXPECT_TRUE(error.IsSuccess());
  }

  {
    Error error;
    // A cleared passphrase should also trigger cache removal.
    EXPECT_CALL(*wifi(), ClearCachedCredentials(wifi_service.get()));
    wifi_service->ClearPassphrase(&error);
    Mock::VerifyAndClearExpectations(wifi());
    EXPECT_TRUE(error.IsSuccess());
  }

  {
    Error error;
    // An invalid passphrase should not trigger cache removal.
    EXPECT_CALL(*wifi(), ClearCachedCredentials(_)).Times(0);
    wifi_service->SetPassphrase("", &error);
    Mock::VerifyAndClearExpectations(wifi());
    EXPECT_FALSE(error.IsSuccess());
  }

  {
    // Any change to EAP parameters (including a null one) will trigger cache
    // removal.  This is a lot less granular than the passphrase checks above.
    EXPECT_CALL(*wifi(), ClearCachedCredentials(wifi_service.get()));
    wifi_service->set_eap(EapCredentials());
    Mock::VerifyAndClearExpectations(wifi());
  }
}

TEST_F(WiFiServiceTest, LoadHidden) {
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityNone);
  ASSERT_FALSE(service->hidden_ssid_);
  NiceMock<MockStore> mock_store;
  const string storage_id = service->GetStorageIdentifier();
  set<string> groups;
  groups.insert(storage_id);
  EXPECT_CALL(mock_store, ContainsGroup(StrEq(storage_id)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_store, GetGroupsWithProperties(
      ContainsWiFiProperties(
          simple_ssid(), flimflam::kModeManaged, flimflam::kSecurityNone)))
      .WillRepeatedly(Return(groups));
  EXPECT_CALL(mock_store, GetBool(_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_store,
              GetBool(StrEq(storage_id), WiFiService::kStorageHiddenSSID, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(true), Return(true)));
  EXPECT_TRUE(service->Load(&mock_store));
  EXPECT_TRUE(service->hidden_ssid_);
}

TEST_F(WiFiServiceTest, LoadMultipleMatchingGroups) {
  WiFiServiceRefPtr service = MakeServiceWithWiFi(flimflam::kSecurityNone);
  set<string> groups;
  groups.insert("id0");
  groups.insert("id1");
  // Make sure we retain the first matched group in the same way that
  // WiFiService::Load() will.
  string first_group = *groups.begin();

  NiceMock<MockStore> mock_store;
  EXPECT_CALL(mock_store, GetGroupsWithProperties(
      ContainsWiFiProperties(
          simple_ssid(), flimflam::kModeManaged, flimflam::kSecurityNone)))
      .WillRepeatedly(Return(groups));
  EXPECT_CALL(mock_store, ContainsGroup(first_group))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_store, ContainsGroup(StrNe(first_group))).Times(0);
  EXPECT_CALL(mock_store, GetBool(first_group, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_store, GetBool(StrNe(first_group), _, _)).Times(0);
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_WARNING, _,
                       EndsWith("choosing the first.")));
  EXPECT_TRUE(service->Load(&mock_store));
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
                              false));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityRsn,
                              flimflam::kSecurityWpa,
                              false));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityWpa,
                              flimflam::kSecurityPsk,
                              true));
  EXPECT_TRUE(TestLoadMapping(flimflam::kSecurityWpa,
                              flimflam::kSecurityWpa,
                              false));
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
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityPsk);
  NiceMock<MockStore> mock_store;
  const string storage_id = service->GetStorageIdentifier();
  EXPECT_CALL(mock_store, ContainsGroup(StrEq(storage_id)))
      .WillRepeatedly(Return(true));
  set<string> groups;
  groups.insert(storage_id);
  EXPECT_CALL(mock_store, GetGroupsWithProperties(
      ContainsWiFiProperties(
          simple_ssid(), flimflam::kModeManaged, flimflam::kSecurityPsk)))
      .WillRepeatedly(Return(groups));
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

TEST_F(WiFiServiceTest, ConfigureMakesConnectable) {
  string guid("legit_guid");
  KeyValueStore args;
  args.SetString(flimflam::kEapIdentityProperty, "legit_identity");
  args.SetString(flimflam::kEapPasswordProperty, "legit_password");
  args.SetString(flimflam::kEAPEAPProperty, "PEAP");
  args.SetString(flimflam::kGuidProperty, guid);
  Error error;

  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurity8021x);
  // Hack the GUID in so that we don't have to mess about with WiFi to regsiter
  // our service.  This way, Manager will handle the lookup itself.
  service->set_guid(guid);
  manager()->RegisterService(service);
  EXPECT_FALSE(service->connectable());
  EXPECT_EQ(service.get(), manager()->GetService(args, &error).get());
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(service->connectable());
}

TEST_F(WiFiServiceTest, ConfigurePassphrase) {
  EXPECT_EQ(Error::kNotSupported,
            TestConfigurePassphrase(flimflam::kSecurityNone, ""));
  EXPECT_EQ(Error::kNotSupported,
            TestConfigurePassphrase(flimflam::kSecurityNone, "foo"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep, NULL));
  EXPECT_EQ(Error::kInvalidPassphrase,
            TestConfigurePassphrase(flimflam::kSecurityWep, ""));
  EXPECT_EQ(Error::kInvalidPassphrase,
            TestConfigurePassphrase(flimflam::kSecurityWep, "abcd"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep, "abcde"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep, "abcdefghijklm"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep, "0:abcdefghijklm"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep, "0102030405"));
  EXPECT_EQ(Error::kInvalidPassphrase,
            TestConfigurePassphrase(flimflam::kSecurityWep, "0x0102030405"));
  EXPECT_EQ(Error::kInvalidPassphrase,
            TestConfigurePassphrase(flimflam::kSecurityWep, "O102030405"));
  EXPECT_EQ(Error::kInvalidPassphrase,
            TestConfigurePassphrase(flimflam::kSecurityWep, "1:O102030405"));
  EXPECT_EQ(Error::kInvalidPassphrase,
            TestConfigurePassphrase(flimflam::kSecurityWep, "1:0xO102030405"));
  EXPECT_EQ(Error::kInvalidPassphrase,
            TestConfigurePassphrase(flimflam::kSecurityWep, "0xO102030405"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep,
                                    "0102030405060708090a0b0c0d"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep,
                                    "0102030405060708090A0B0C0D"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep,
                                    "0:0102030405060708090a0b0c0d"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWep,
                                    "0:0x0102030405060708090a0b0c0d"));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWpa, NULL));
  EXPECT_EQ(Error::kSuccess,
            TestConfigurePassphrase(flimflam::kSecurityWpa, "secure password"));
  EXPECT_EQ(Error::kInvalidPassphrase,
            TestConfigurePassphrase(flimflam::kSecurityWpa, ""));
  EXPECT_EQ(Error::kSuccess, TestConfigurePassphrase(
      flimflam::kSecurityWpa,
      string(IEEE_80211::kWPAAsciiMinLen, 'Z').c_str()));
  EXPECT_EQ(Error::kSuccess, TestConfigurePassphrase(
      flimflam::kSecurityWpa,
      string(IEEE_80211::kWPAAsciiMaxLen, 'Z').c_str()));
  // subtle: invalid length for hex key, but valid as ascii passphrase
  EXPECT_EQ(Error::kSuccess, TestConfigurePassphrase(
      flimflam::kSecurityWpa,
      string(IEEE_80211::kWPAHexLen-1, '1').c_str()));
  EXPECT_EQ(Error::kSuccess, TestConfigurePassphrase(
      flimflam::kSecurityWpa,
      string(IEEE_80211::kWPAHexLen, '1').c_str()));
  EXPECT_EQ(Error::kInvalidPassphrase, TestConfigurePassphrase(
      flimflam::kSecurityWpa,
      string(IEEE_80211::kWPAAsciiMinLen-1, 'Z').c_str()));
  EXPECT_EQ(Error::kInvalidPassphrase, TestConfigurePassphrase(
      flimflam::kSecurityWpa,
      string(IEEE_80211::kWPAAsciiMaxLen+1, 'Z').c_str()));
  EXPECT_EQ(Error::kInvalidPassphrase, TestConfigurePassphrase(
      flimflam::kSecurityWpa,
      string(IEEE_80211::kWPAHexLen+1, '1').c_str()));
}

TEST_F(WiFiServiceTest, ConfigureRedundantProperties) {
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityNone);
  KeyValueStore args;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWifi);
  args.SetString(flimflam::kSSIDProperty, simple_ssid_string());
  args.SetString(flimflam::kSecurityProperty, flimflam::kSecurityNone);
  const string kGUID = "aguid";
  args.SetString(flimflam::kGuidProperty, kGUID);

  EXPECT_EQ("", service->guid());
  Error error;
  service->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kGUID, service->guid());
}

TEST_F(WiFiServiceTest, DisconnectWithWiFi) {
  WiFiServiceRefPtr service = MakeServiceWithWiFi(flimflam::kSecurityWep);
  EXPECT_CALL(*wifi(), DisconnectFrom(service.get())).Times(1);
  Error error;
  service->Disconnect(&error);
}

TEST_F(WiFiServiceTest, DisconnectWithoutWiFi) {
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityWep);
  EXPECT_CALL(*wifi(), DisconnectFrom(_)).Times(0);
  Error error;
  service->Disconnect(&error);
  EXPECT_EQ(Error::kOperationFailed, error.type());
}

TEST_F(WiFiServiceTest, DisconnectWithoutWiFiWhileAssociating) {
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityWep);
  EXPECT_CALL(*wifi(), DisconnectFrom(_)).Times(0);
  service->SetState(Service::kStateAssociating);
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       HasSubstr("WiFi endpoints do not (yet) exist.")));
  Error error;
  service->Disconnect(&error);
  EXPECT_EQ(Error::kOperationFailed, error.type());
}

TEST_F(WiFiServiceTest, UnloadAndClearCacheWEP) {
  WiFiServiceRefPtr service = MakeServiceWithWiFi(flimflam::kSecurityWep);
  EXPECT_CALL(*wifi(), ClearCachedCredentials(service.get())).Times(1);
  EXPECT_CALL(*wifi(), DisconnectFrom(service.get())).Times(1);
  service->Unload();
}

TEST_F(WiFiServiceTest, UnloadAndClearCache8021x) {
  WiFiServiceRefPtr service = MakeServiceWithWiFi(flimflam::kSecurity8021x);
  EXPECT_CALL(*wifi(), ClearCachedCredentials(service.get())).Times(1);
  EXPECT_CALL(*wifi(), DisconnectFrom(service.get())).Times(1);
  service->Unload();
}

TEST_F(WiFiServiceTest, ParseStorageIdentifierNone) {
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityNone);
  const string storage_id = service->GetStorageIdentifier();
  string address;
  string mode;
  string security;
  EXPECT_TRUE(service->ParseStorageIdentifier(storage_id, &address, &mode,
                                              &security));
  EXPECT_EQ(StringToLowerASCII(GetAnyDeviceAddress()), address);
  EXPECT_EQ(flimflam::kModeManaged, mode);
  EXPECT_EQ(flimflam::kSecurityNone, security);
}

TEST_F(WiFiServiceTest, ParseStorageIdentifier8021x) {
  // Do a separate test for 802.1x, since kSecurity8021x contains a "_",
  // which needs to be dealt with specially in the parser.
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurity8021x);
  const string storage_id = service->GetStorageIdentifier();
  string address;
  string mode;
  string security;
  EXPECT_TRUE(service->ParseStorageIdentifier(storage_id, &address, &mode,
                                              &security));
  EXPECT_EQ(StringToLowerASCII(GetAnyDeviceAddress()), address);
  EXPECT_EQ(flimflam::kModeManaged, mode);
  EXPECT_EQ(flimflam::kSecurity8021x, security);
}

TEST_F(WiFiServiceFixupStorageTest, FixedEntries) {
  const string kNonWiFiId = "vpn_foo";
  const string kUnparsableWiFiId = "wifi_foo";

  AddGroup(kNonWiFiId);
  AddGroup(kUnparsableWiFiId);
  AddServiceEntry(true, true, true, true);
  AddServiceEntry(false, false, false, false);
  AddServiceEntry(true, true, true, true);
  AddServiceEntry(false, false, false, false);
  EXPECT_TRUE(FixupServiceEntries());
}

TEST_F(WiFiServiceFixupStorageTest, NoFixedEntries) {
  const string kNonWiFiId = "vpn_foo";
  const string kUnparsableWiFiId = "wifi_foo";

  AddGroup(kNonWiFiId);
  AddGroup(kUnparsableWiFiId);
  AddServiceEntry(true, true, true, true);
  EXPECT_FALSE(FixupServiceEntries());
}

TEST_F(WiFiServiceFixupStorageTest, MissingTypeProperty) {
  AddServiceEntry(false, true, true, true);
  EXPECT_TRUE(FixupServiceEntries());
}

TEST_F(WiFiServiceFixupStorageTest, MissingModeProperty) {
  AddServiceEntry(true, false, true, true);
  EXPECT_TRUE(FixupServiceEntries());
}

TEST_F(WiFiServiceFixupStorageTest, MissingSecurityProperty) {
  AddServiceEntry(true, true, false, true);
  EXPECT_TRUE(FixupServiceEntries());
}

TEST_F(WiFiServiceFixupStorageTest, MissingSecurityClassProperty) {
  AddServiceEntry(true, true, true, false);
  EXPECT_TRUE(FixupServiceEntries());
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

  EapCredentials eap;
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
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityNone);
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(service->HasEndpoints());
  EXPECT_FALSE(service->IsAutoConnectable(&reason));
  EXPECT_STREQ(WiFiService::kAutoConnNoEndpoint, reason);

  reason = "";
  WiFiEndpointRefPtr endpoint =
      MakeOpenEndpoint("a", "00:00:00:00:00:01", 0, 0);
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
  WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityNone);
  EXPECT_FALSE(service->IsAutoConnectable(&reason));
  EXPECT_CALL(*wifi(), ConnectTo(_, _))
      .Times(0);
  service->AutoConnect();
  dispatcher()->DispatchPendingEvents();

  WiFiEndpointRefPtr endpoint =
      MakeOpenEndpoint("a", "00:00:00:00:00:01", 0, 0);
  service->AddEndpoint(endpoint);
  EXPECT_CALL(*wifi(), IsIdle())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service->IsAutoConnectable(&reason));
  EXPECT_CALL(*wifi(), ConnectTo(_, _));
  service->AutoConnect();
  dispatcher()->DispatchPendingEvents();

  Error error;
  service->UserInitiatedDisconnect(&error);
  dispatcher()->DispatchPendingEvents();
  EXPECT_FALSE(service->IsAutoConnectable(&reason));
}

TEST_F(WiFiServiceTest, ClearWriteOnlyDerivedProperty) {
  WiFiServiceRefPtr wifi_service = MakeSimpleService(flimflam::kSecurityWep);

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
  EXPECT_CALL(adaptor, EmitStringChanged(
      flimflam::kWifiBSsid, kOkEndpointBssId));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kOkEndpointStrength));
  service->AddEndpoint(ok_endpoint);
  EXPECT_EQ(1, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);

  // Endpoint with stronger signal updates values.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kGoodEndpointFrequency));
  EXPECT_CALL(adaptor, EmitStringChanged(
      flimflam::kWifiBSsid, kGoodEndpointBssId));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kGoodEndpointStrength));
  service->AddEndpoint(good_endpoint);
  EXPECT_EQ(2, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);

  // Endpoint with lower signal does not change values.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor, EmitStringChanged(flimflam::kWifiBSsid, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->AddEndpoint(bad_endpoint);
  EXPECT_EQ(3, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing non-optimal endpoint does not change values.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor, EmitStringChanged(flimflam::kWifiBSsid, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->RemoveEndpoint(bad_endpoint);
  EXPECT_EQ(2, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing optimal endpoint updates values.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kOkEndpointFrequency));
  EXPECT_CALL(adaptor, EmitStringChanged(
      flimflam::kWifiBSsid, kOkEndpointBssId));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kOkEndpointStrength));
  service->RemoveEndpoint(good_endpoint);
  EXPECT_EQ(1, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing last endpoint updates values (and doesn't crash).
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _));
  EXPECT_CALL(adaptor, EmitStringChanged(flimflam::kWifiBSsid, _));
  EXPECT_CALL(adaptor, EmitUint8Changed(flimflam::kSignalStrengthProperty, _));
  service->RemoveEndpoint(ok_endpoint);
  EXPECT_EQ(0, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);
}

TEST_F(WiFiServiceUpdateFromEndpointsTest, Connected) {
  EXPECT_CALL(adaptor, EmitUint16Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitStringChanged(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitUint8Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitBoolChanged(_, _)).Times(AnyNumber());
  service->AddEndpoint(bad_endpoint);
  service->AddEndpoint(ok_endpoint);
  EXPECT_EQ(2, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);

  // Setting current endpoint forces adoption of its values, even if it
  // doesn't have the highest signal.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kBadEndpointFrequency));
  EXPECT_CALL(adaptor, EmitStringChanged(
      flimflam::kWifiBSsid, kBadEndpointBssId));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kBadEndpointStrength));
  service->NotifyCurrentEndpoint(bad_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Adding a better endpoint doesn't matter, when current endpoint is set.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor, EmitStringChanged(flimflam::kWifiBSsid, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->AddEndpoint(good_endpoint);
  EXPECT_EQ(3, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing a better endpoint doesn't matter, when current endpoint is set.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor, EmitStringChanged(flimflam::kWifiBSsid, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->RemoveEndpoint(good_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Removing the current endpoint is safe and sane.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kOkEndpointFrequency));
  EXPECT_CALL(adaptor, EmitStringChanged(
      flimflam::kWifiBSsid, kOkEndpointBssId));
  EXPECT_CALL(adaptor, EmitUint8Changed(
      flimflam::kSignalStrengthProperty, kOkEndpointStrength));
  service->RemoveEndpoint(bad_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Clearing the current endpoint (without removing it) is also safe and sane.
  service->NotifyCurrentEndpoint(ok_endpoint);
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor, EmitStringChanged(flimflam::kWifiBSsid, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  service->NotifyCurrentEndpoint(NULL);
  Mock::VerifyAndClearExpectations(&adaptor);
}

TEST_F(WiFiServiceUpdateFromEndpointsTest, EndpointModified) {
  EXPECT_CALL(adaptor, EmitUint16Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitStringChanged(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitUint8Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitBoolChanged(_, _)).Times(AnyNumber());
  service->AddEndpoint(ok_endpoint);
  service->AddEndpoint(good_endpoint);
  EXPECT_EQ(2, service->GetEndpointCount());
  Mock::VerifyAndClearExpectations(&adaptor);

  // Updating sub-optimal Endpoint doesn't update Service.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor, EmitStringChanged(flimflam::kWifiBSsid, _)).Times(0);
  EXPECT_CALL(adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _)).Times(0);
  ok_endpoint->signal_strength_ = (kOkEndpointSignal + kGoodEndpointSignal) / 2;
  service->NotifyEndpointUpdated(ok_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Updating optimal Endpoint updates appropriate Service property.
  EXPECT_CALL(adaptor, EmitUint16Changed(flimflam::kWifiFrequency, _)).Times(0);
  EXPECT_CALL(adaptor, EmitStringChanged(flimflam::kWifiBSsid, _)).Times(0);
  EXPECT_CALL(adaptor, EmitUint8Changed(flimflam::kSignalStrengthProperty, _));
  good_endpoint->signal_strength_ = kGoodEndpointSignal + 1;
  service->NotifyEndpointUpdated(good_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);

  // Change in optimal Endpoint updates Service properties.
  EXPECT_CALL(adaptor, EmitUint16Changed(
      flimflam::kWifiFrequency, kOkEndpointFrequency));
  EXPECT_CALL(adaptor, EmitStringChanged(
      flimflam::kWifiBSsid, kOkEndpointBssId));
  EXPECT_CALL(adaptor, EmitUint8Changed(flimflam::kSignalStrengthProperty, _));
  ok_endpoint->signal_strength_ = kGoodEndpointSignal + 2;
  service->NotifyEndpointUpdated(ok_endpoint);
  Mock::VerifyAndClearExpectations(&adaptor);
}

TEST_F(WiFiServiceUpdateFromEndpointsTest, Ieee80211w) {
  EXPECT_CALL(adaptor, EmitUint16Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitStringChanged(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitUint8Changed(_, _)).Times(AnyNumber());
  EXPECT_CALL(adaptor, EmitBoolChanged(_, _)).Times(AnyNumber());
  service->AddEndpoint(ok_endpoint);
  EXPECT_FALSE(service->ieee80211w_required());
  good_endpoint->ieee80211w_required_ = true;
  service->AddEndpoint(good_endpoint);
  EXPECT_TRUE(service->ieee80211w_required());
  service->RemoveEndpoint(good_endpoint);
  EXPECT_TRUE(service->ieee80211w_required());
}

TEST_F(WiFiServiceUpdateFromEndpointsTest, WarningOnDisconnect) {
  service->AddEndpoint(ok_endpoint);
  service->SetState(Service::kStateAssociating);
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_WARNING, _,
                       EndsWith("disconnect due to no remaining endpoints.")));
  service->RemoveEndpoint(ok_endpoint);
}

TEST_F(WiFiServiceTest, SecurityFromCurrentEndpoint) {
  WiFiServiceRefPtr service(MakeSimpleService(flimflam::kSecurityPsk));
  EXPECT_EQ(flimflam::kSecurityPsk, service->GetSecurity(NULL));
  WiFiEndpoint *endpoint = MakeOpenEndpoint(
        simple_ssid_string(), "00:00:00:00:00:00", 0, 0);
  service->AddEndpoint(endpoint);
  EXPECT_EQ(flimflam::kSecurityPsk, service->GetSecurity(NULL));
  service->NotifyCurrentEndpoint(endpoint);
  EXPECT_EQ(flimflam::kSecurityNone, service->GetSecurity(NULL));
  service->NotifyCurrentEndpoint(NULL);
  EXPECT_EQ(flimflam::kSecurityPsk, service->GetSecurity(NULL));
}

TEST_F(WiFiServiceTest, UpdateSecurity) {
  // Cleartext and pre-shared-key crypto.
  {
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityNone);
    EXPECT_EQ(Service::kCryptoNone, service->crypto_algorithm());
    EXPECT_FALSE(service->key_rotation());
    EXPECT_FALSE(service->endpoint_auth());
  }
  {
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityWep);
    EXPECT_EQ(Service::kCryptoRc4, service->crypto_algorithm());
    EXPECT_FALSE(service->key_rotation());
    EXPECT_FALSE(service->endpoint_auth());
  }
  {
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityPsk);
    EXPECT_EQ(Service::kCryptoRc4, service->crypto_algorithm());
    EXPECT_TRUE(service->key_rotation());
    EXPECT_FALSE(service->endpoint_auth());
  }
  {
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityWpa);
    EXPECT_EQ(Service::kCryptoRc4, service->crypto_algorithm());
    EXPECT_TRUE(service->key_rotation());
    EXPECT_FALSE(service->endpoint_auth());
  }
  {
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityRsn);
    EXPECT_EQ(Service::kCryptoAes, service->crypto_algorithm());
    EXPECT_TRUE(service->key_rotation());
    EXPECT_FALSE(service->endpoint_auth());
  }

  // Crypto with 802.1X key management.
  {
    // WEP
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurityWep);
    service->SetEAPKeyManagement("IEEE8021X");
    EXPECT_EQ(Service::kCryptoRc4, service->crypto_algorithm());
    EXPECT_TRUE(service->key_rotation());
    EXPECT_TRUE(service->endpoint_auth());
  }
  {
    // WPA
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurity8021x);
    WiFiEndpointRefPtr endpoint =
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, true, false);
    service->AddEndpoint(endpoint);
    EXPECT_EQ(Service::kCryptoRc4, service->crypto_algorithm());
    EXPECT_TRUE(service->key_rotation());
    EXPECT_TRUE(service->endpoint_auth());
  }
  {
    // RSN
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurity8021x);
    WiFiEndpointRefPtr endpoint =
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, false, true);
    service->AddEndpoint(endpoint);
    EXPECT_EQ(Service::kCryptoAes, service->crypto_algorithm());
    EXPECT_TRUE(service->key_rotation());
    EXPECT_TRUE(service->endpoint_auth());
  }
  {
    // AP supports both WPA and RSN.
    WiFiServiceRefPtr service = MakeSimpleService(flimflam::kSecurity8021x);
    WiFiEndpointRefPtr endpoint =
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, true, true);
    service->AddEndpoint(endpoint);
    EXPECT_EQ(Service::kCryptoAes, service->crypto_algorithm());
    EXPECT_TRUE(service->key_rotation());
    EXPECT_TRUE(service->endpoint_auth());
  }
}

TEST_F(WiFiServiceTest, ComputeCipher8021x) {
  // No endpoints.
  {
    const set<WiFiEndpointConstRefPtr> endpoints;
    EXPECT_EQ(Service::kCryptoNone,
              WiFiService::ComputeCipher8021x(endpoints));
  }

  // Single endpoint, various configs.
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, false, false));
    EXPECT_EQ(Service::kCryptoNone,
              WiFiService::ComputeCipher8021x(endpoints));
  }
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, true, false));
    EXPECT_EQ(Service::kCryptoRc4,
              WiFiService::ComputeCipher8021x(endpoints));
  }
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, false, true));
    EXPECT_EQ(Service::kCryptoAes,
              WiFiService::ComputeCipher8021x(endpoints));
  }
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, true, true));
    EXPECT_EQ(Service::kCryptoAes,
              WiFiService::ComputeCipher8021x(endpoints));
  }

  // Multiple endpoints.
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, false, false));
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:02", 0, 0, false, false));
    EXPECT_EQ(Service::kCryptoNone,
              WiFiService::ComputeCipher8021x(endpoints));
  }
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, false, false));
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:02", 0, 0, true, false));
    EXPECT_EQ(Service::kCryptoNone,
              WiFiService::ComputeCipher8021x(endpoints));
  }
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, true, false));
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:02", 0, 0, true, false));
    EXPECT_EQ(Service::kCryptoRc4,
              WiFiService::ComputeCipher8021x(endpoints));
  }
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, true, false));
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:02", 0, 0, false, true));
    EXPECT_EQ(Service::kCryptoRc4,
              WiFiService::ComputeCipher8021x(endpoints));
  }
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, false, true));
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:02", 0, 0, false, true));
    EXPECT_EQ(Service::kCryptoAes,
              WiFiService::ComputeCipher8021x(endpoints));
  }
  {
    set<WiFiEndpointConstRefPtr> endpoints;
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:01", 0, 0, true, true));
    endpoints.insert(
        MakeEndpoint("a", "00:00:00:00:00:02", 0, 0, true, true));
    EXPECT_EQ(Service::kCryptoAes,
              WiFiService::ComputeCipher8021x(endpoints));
  }
}

TEST_F(WiFiServiceTest, Unload) {
  WiFiServiceRefPtr service = MakeServiceWithWiFi(flimflam::kSecurityNone);
  EXPECT_CALL(*wifi(), DestroyIPConfigLease(service->GetStorageIdentifier())).
    Times(1);
  service->Unload();
}


}  // namespace shill
