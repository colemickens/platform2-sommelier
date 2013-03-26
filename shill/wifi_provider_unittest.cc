// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_provider.h"

#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "shill/ieee80211.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_profile.h"
#include "shill/mock_store.h"
#include "shill/mock_wifi_service.h"
#include "shill/nice_mock_control.h"
#include "shill/technology.h"
#include "shill/wifi_endpoint.h"
#include "shill/wpa_supplicant.h"

using std::set;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace shill {

class WiFiProviderTest : public testing::Test {
 public:
  WiFiProviderTest()
      : metrics_(NULL),
        manager_(&control_, &dispatcher_, &metrics_,
                 reinterpret_cast<GLib *>(NULL)),
        provider_(&control_, &dispatcher_, &metrics_, &manager_),
        profile_(
            new NiceMock<MockProfile>(&control_, &metrics_, &manager_, "")),
        storage_entry_index_(0) {}

  virtual ~WiFiProviderTest() {}

  virtual void SetUp() {
    EXPECT_CALL(*profile_, GetConstStorage()).WillRepeatedly(Return(&storage_));
  }

 protected:
  typedef scoped_refptr<MockWiFiService> MockWiFiServiceRefPtr;

  void CreateServicesFromProfile() {
    provider_.CreateServicesFromProfile(profile_);
  }

  void FixupServiceEntries(bool is_default_profile) {
    provider_.FixupServiceEntries(&storage_, is_default_profile);
  }

  const vector<WiFiServiceRefPtr> GetServices() {
    return provider_.services_;
  }

  bool GetRunning() {
    return provider_.running_;
  }

  void AddStringParameterToStorage(const string &id,
                                   const string &key,
                                   const string &value) {
    EXPECT_CALL(storage_, GetString(id, key, _))
        .WillRepeatedly(DoAll(SetArgumentPointee<2>(value),
                              Return(true)));
  }

  string AddServiceToStorage(const char *ssid,
                             const char *mode,
                             const char *security,
                             bool is_hidden,
                             bool provide_hidden) {
    string id = StringToLowerASCII(base::StringPrintf("entry_%d",
                                                      storage_entry_index_));
    EXPECT_CALL(storage_, GetString(id, _, _)).WillRepeatedly(Return(false));
    if (ssid) {
      const string ssid_string(ssid);
      const string hex_ssid(
          base::HexEncode(ssid_string.data(), ssid_string.size()));
      AddStringParameterToStorage(id, WiFiService::kStorageSSID, hex_ssid);
    }
    if (mode) {
      AddStringParameterToStorage(id, WiFiService::kStorageMode, mode);
    }
    if (security) {
      AddStringParameterToStorage(id, WiFiService::kStorageSecurity, security);
    }
    if (provide_hidden) {
      EXPECT_CALL(storage_, GetBool(id, flimflam::kWifiHiddenSsid, _))
          .WillRepeatedly(
              DoAll(SetArgumentPointee<2>(is_hidden), Return(true)));
    } else {
      EXPECT_CALL(storage_, GetBool(id, flimflam::kWifiHiddenSsid, _))
          .WillRepeatedly(Return(false));
    }
    storage_entry_index_++;
    return id;
  }

  void SetServiceParameters(const char *ssid,
                            const char *mode,
                            const char *security,
                            bool is_hidden,
                            bool provide_hidden,
                            KeyValueStore *args) {
    args->SetString(flimflam::kTypeProperty, flimflam::kTypeWifi);
    if (ssid) {
      args->SetString(flimflam::kSSIDProperty, ssid);
    }
    if (mode) {
      args->SetString(flimflam::kModeProperty, mode);
    }
    if (security) {
      args->SetString(flimflam::kSecurityProperty, security);
    }
    if (provide_hidden) {
      args->SetBool(flimflam::kWifiHiddenSsid, is_hidden);
    }
  }

  WiFiServiceRefPtr CreateTemporaryService(const char *ssid,
                                           const char *mode,
                                           const char *security,
                                           bool is_hidden,
                                           bool provide_hidden,
                                           Error *error) {
    KeyValueStore args;
    SetServiceParameters(
        ssid, mode, security, is_hidden, provide_hidden, &args);
    return provider_.CreateTemporaryService(args, error);
  }

  WiFiServiceRefPtr GetService(const char *ssid,
                               const char *mode,
                               const char *security,
                               bool is_hidden,
                               bool provide_hidden,
                               Error *error) {
    KeyValueStore args;
    SetServiceParameters(
        ssid, mode, security, is_hidden, provide_hidden, &args);
    return provider_.GetService(args, error);
  }

  WiFiServiceRefPtr FindService(const vector<uint8_t> &ssid,
                                const string &mode,
                                const string &security) {
    return provider_.FindService(ssid, mode, security);
  }
  WiFiEndpointRefPtr MakeEndpoint(const string &ssid, const string &bssid,
                                  uint16 frequency, int16 signal_dbm) {
    return WiFiEndpoint::MakeOpenEndpoint(
        NULL, NULL, ssid, bssid, WPASupplicant::kNetworkModeInfrastructure,
        frequency, signal_dbm);
  }
  MockWiFiServiceRefPtr AddMockService(const vector<uint8_t> &ssid,
                                       const string &mode,
                                       const string &security,
                                       bool hidden_ssid) {
    MockWiFiServiceRefPtr service = new MockWiFiService(
        &control_,
        &dispatcher_,
        &metrics_,
        &manager_,
        &provider_,
        ssid,
        mode,
        security,
        hidden_ssid);
    provider_.services_.push_back(service);
    return service;
  }
  NiceMockControl control_;
  MockEventDispatcher dispatcher_;
  MockMetrics metrics_;
  StrictMock<MockManager> manager_;
  WiFiProvider provider_;
  scoped_refptr<MockProfile> profile_;
  StrictMock<MockStore> storage_;
  int storage_entry_index_;
};

MATCHER(TypeWiFiPropertyMatch, "") {
  return
      arg.bool_properties().empty() &&
      arg.int_properties().empty() &&
      arg.uint_properties().empty() &&
      arg.string_properties().size() == 1 &&
      arg.LookupString(flimflam::kTypeProperty, "") == flimflam::kTypeWifi;
}

MATCHER_P(RefPtrMatch, ref, "") {
  return ref.get() == arg.get();
}

TEST_F(WiFiProviderTest, Start) {
  // Doesn't do anything really.  Just testing for no crash.
  EXPECT_TRUE(GetServices().empty());
  EXPECT_FALSE(GetRunning());
  provider_.Start();
  EXPECT_TRUE(GetServices().empty());
  EXPECT_TRUE(GetRunning());
}

TEST_F(WiFiProviderTest, Stop) {
  MockWiFiServiceRefPtr service0 = AddMockService(vector<uint8_t>(1, '0'),
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  MockWiFiServiceRefPtr service1 = AddMockService(vector<uint8_t>(1, '1'),
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  EXPECT_EQ(2, GetServices().size());
  EXPECT_CALL(*service0, ResetWiFi()).Times(1);
  EXPECT_CALL(*service1, ResetWiFi()).Times(1);
  EXPECT_CALL(manager_, DeregisterService(RefPtrMatch(service0))).Times(1);
  EXPECT_CALL(manager_, DeregisterService(RefPtrMatch(service1))).Times(1);
  provider_.Stop();
  // Verify now, so it's clear that this happened as a result of the call
  // above, and not anything in the destructor(s).
  Mock::VerifyAndClearExpectations(service0);
  Mock::VerifyAndClearExpectations(service1);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileWithNoGroups) {
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillOnce(Return(set<string>()));
  CreateServicesFromProfile();
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileMissingSSID) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  groups.insert(AddServiceToStorage(
      NULL, flimflam::kModeManaged, flimflam::kSecurityNone, false, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  CreateServicesFromProfile();
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileEmptySSID) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  groups.insert(AddServiceToStorage(
      "", flimflam::kModeManaged, flimflam::kSecurityNone, false, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  CreateServicesFromProfile();
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileMissingMode) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  groups.insert(AddServiceToStorage(
      "foo", NULL, flimflam::kSecurityNone, false, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  CreateServicesFromProfile();
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileEmptyMode) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  groups.insert(AddServiceToStorage(
      "foo", "", flimflam::kSecurityNone, false, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  CreateServicesFromProfile();
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileMissingSecurity) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  groups.insert(AddServiceToStorage(
      "foo", flimflam::kModeManaged, NULL, false, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  CreateServicesFromProfile();
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileEmptySecurity) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  groups.insert(AddServiceToStorage(
      "foo", flimflam::kModeManaged, "", false, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  CreateServicesFromProfile();
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileMissingHidden) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  groups.insert(AddServiceToStorage(
      "foo", flimflam::kModeManaged, flimflam::kSecurityNone, false, false));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  CreateServicesFromProfile();
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileSingle) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  string kSSID("foo");
  groups.insert(AddServiceToStorage(
      kSSID.c_str(), flimflam::kModeManaged, flimflam::kSecurityNone,
      false, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  CreateServicesFromProfile();
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_EQ(1, GetServices().size());

  const WiFiServiceRefPtr service = GetServices().front();
  const string service_ssid(service->ssid().begin(), service->ssid().end());
  EXPECT_EQ(kSSID, service_ssid);
  EXPECT_EQ(flimflam::kModeManaged, service->mode());
  EXPECT_TRUE(service->IsSecurityMatch(flimflam::kSecurityNone));

  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  CreateServicesFromProfile();
  EXPECT_EQ(1, GetServices().size());
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileHiddenButConnected) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  string kSSID("foo");
  groups.insert(AddServiceToStorage(
      kSSID.c_str(), flimflam::kModeManaged, flimflam::kSecurityNone,
      true, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_CALL(manager_, IsTechnologyConnected(Technology::kWifi))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, RequestScan(_, _)).Times(0);
  CreateServicesFromProfile();
  Mock::VerifyAndClearExpectations(&manager_);

  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, IsTechnologyConnected(_)).Times(0);
  CreateServicesFromProfile();
}

TEST_F(WiFiProviderTest, CreateServicesFromProfileHiddenNotConnected) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  string kSSID("foo");
  groups.insert(AddServiceToStorage(
      kSSID.c_str(), flimflam::kModeManaged, flimflam::kSecurityNone,
      true, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_CALL(manager_, IsTechnologyConnected(Technology::kWifi))
      .WillOnce(Return(false));
  EXPECT_CALL(manager_, RequestScan(flimflam::kTypeWifi, _)).Times(1);
  CreateServicesFromProfile();
  Mock::VerifyAndClearExpectations(&manager_);

  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, IsTechnologyConnected(_)).Times(0);
  EXPECT_CALL(manager_, RequestScan(_, _)).Times(0);
  CreateServicesFromProfile();
}

TEST_F(WiFiProviderTest, CreateTwoServices) {
  string id;
  StrictMock<MockStore> storage;
  set<string> groups;
  groups.insert(AddServiceToStorage(
      "foo", flimflam::kModeManaged, flimflam::kSecurityNone, false, true));
  groups.insert(AddServiceToStorage(
      "bar", flimflam::kModeManaged, flimflam::kSecurityNone, true, true));
  EXPECT_CALL(storage_, GetGroupsWithProperties(TypeWiFiPropertyMatch()))
      .WillRepeatedly(Return(groups));
  EXPECT_CALL(manager_, RegisterService(_)).Times(2);
  EXPECT_CALL(manager_, IsTechnologyConnected(Technology::kWifi))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, RequestScan(flimflam::kTypeWifi, _)).Times(0);
  CreateServicesFromProfile();
  Mock::VerifyAndClearExpectations(&manager_);

  EXPECT_EQ(2, GetServices().size());
}

TEST_F(WiFiProviderTest, GetServiceEmptyMode) {
  Error error;
  EXPECT_FALSE(GetService("foo", "", flimflam::kSecurityNone,
                          false, false, &error).get());
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(WiFiProviderTest, GetServiceNoMode) {
  Error error;
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_TRUE(GetService("foo", NULL, flimflam::kSecurityNone,
                          false, false, &error).get());
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(WiFiProviderTest, GetServiceBadMode) {
  Error error;
  EXPECT_FALSE(GetService("foo", "BogoMesh",
                          flimflam::kSecurityNone,
                          false, false, &error).get());
  EXPECT_EQ(Error::kNotSupported, error.type());
  EXPECT_EQ("service mode is unsupported", error.message());
}

TEST_F(WiFiProviderTest, GetServiceNoSSID) {
  Error error;
  EXPECT_FALSE(GetService(NULL, flimflam::kModeManaged,
                          flimflam::kSecurityNone, false, false,
                          &error).get());
  EXPECT_EQ(Error::kInvalidArguments, error.type());
  EXPECT_EQ("must specify SSID", error.message());
}

TEST_F(WiFiProviderTest, GetServiceEmptySSID) {
  Error error;
  EXPECT_FALSE(GetService("", flimflam::kModeManaged,
                          flimflam::kSecurityNone, false, false,
                          &error).get());
  EXPECT_EQ(Error::kInvalidNetworkName, error.type());
  EXPECT_EQ("SSID is too short", error.message());
}

TEST_F(WiFiProviderTest, GetServiceLongSSID) {
  Error error;
  string ssid(IEEE_80211::kMaxSSIDLen + 1, '0');
  EXPECT_FALSE(GetService(ssid.c_str(), flimflam::kModeManaged,
                          flimflam::kSecurityNone, false, false,
                          &error).get());
  EXPECT_EQ(Error::kInvalidNetworkName, error.type());
  EXPECT_EQ("SSID is too long", error.message());
}

TEST_F(WiFiProviderTest, GetServiceJustLongEnoughSSID) {
  Error error;
  string ssid(IEEE_80211::kMaxSSIDLen, '0');
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_TRUE(GetService(ssid.c_str(), flimflam::kModeManaged,
                         flimflam::kSecurityNone, false, false,
                         &error));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(WiFiProviderTest, GetServiceBadSecurty) {
  Error error;
  EXPECT_FALSE(GetService("foo", flimflam::kModeManaged,
                          "pig-80211", false, false,
                          &error));
  EXPECT_EQ(Error::kNotSupported, error.type());
  EXPECT_EQ("security mode is unsupported", error.message());
}

TEST_F(WiFiProviderTest, GetServiceMinimal) {
  Error error;
  const string kSSID("foo");
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  WiFiServiceRefPtr service = GetService(kSSID.c_str(), flimflam::kModeManaged,
                                         NULL, false, false, &error);
  EXPECT_TRUE(service.get());
  EXPECT_TRUE(error.IsSuccess());
  const string service_ssid(service->ssid().begin(), service->ssid().end());
  EXPECT_EQ(kSSID, service_ssid);
  EXPECT_EQ(flimflam::kModeManaged, service->mode());

  // These two should be set to their default values if not specified.
  EXPECT_TRUE(service->IsSecurityMatch(flimflam::kSecurityNone));
  EXPECT_TRUE(service->hidden_ssid());
}

TEST_F(WiFiProviderTest, GetServiceFullySpecified) {
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  const string kSSID("bar");
  Error error;
  WiFiServiceRefPtr service0 =
      GetService(kSSID.c_str(), flimflam::kModeManaged,
                 flimflam::kSecurityPsk, false, true, &error);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_TRUE(error.IsSuccess());
  const string service_ssid(service0->ssid().begin(), service0->ssid().end());
  EXPECT_EQ(kSSID, service_ssid);
  EXPECT_EQ(flimflam::kModeManaged, service0->mode());
  EXPECT_TRUE(service0->IsSecurityMatch(flimflam::kSecurityPsk));
  EXPECT_FALSE(service0->hidden_ssid());

  // Getting the same service parameters (even with a different hidden
  // parameter) should return the same service.
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiFiServiceRefPtr service1 =
      GetService(kSSID.c_str(), flimflam::kModeManaged,
                 flimflam::kSecurityPsk, true, true, &error);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_EQ(service0.get(), service1.get());
  EXPECT_EQ(1, GetServices().size());

  // Getting the same ssid with different other parameters should return
  // a different service.
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  WiFiServiceRefPtr service2 =
      GetService(kSSID.c_str(), flimflam::kModeManaged,
                 flimflam::kSecurityNone, true, true, &error);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_NE(service0.get(), service2.get());
  EXPECT_EQ(2, GetServices().size());
}

TEST_F(WiFiProviderTest, FindSimilarService) {
  // Since CreateTemporyService uses exactly the same validation as
  // GetService, don't bother with testing invalid parameters.
  const string kSSID("foo");
  KeyValueStore args;
  SetServiceParameters(
      kSSID.c_str(), flimflam::kModeManaged, flimflam::kSecurityNone,
      true, true, &args);
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  Error get_service_error;
  WiFiServiceRefPtr service = provider_.GetService(args, &get_service_error);
  EXPECT_EQ(1, GetServices().size());

  {
    Error error;
    WiFiServiceRefPtr find_service = provider_.FindSimilarService(args, &error);
    EXPECT_EQ(service.get(), find_service.get());
    EXPECT_TRUE(error.IsSuccess());
  }

  args.SetBool(flimflam::kWifiHiddenSsid, false);

  {
    Error error;
    WiFiServiceRefPtr find_service = provider_.FindSimilarService(args, &error);
    EXPECT_EQ(service.get(), find_service.get());
    EXPECT_TRUE(error.IsSuccess());
  }

  args.SetString(flimflam::kSecurityProperty, flimflam::kSecurityWpa);

  {
    Error error;
    WiFiServiceRefPtr find_service = provider_.FindSimilarService(args, &error);
    EXPECT_EQ(NULL, find_service.get());
    EXPECT_EQ(Error::kNotFound, error.type());
  }
}

TEST_F(WiFiProviderTest, CreateTemporaryService) {
  // Since CreateTemporyService uses exactly the same validation as
  // GetService, don't bother with testing invalid parameters.
  const string kSSID("foo");
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  Error error;
  WiFiServiceRefPtr service0 =
      GetService(kSSID.c_str(), flimflam::kModeManaged,
                 flimflam::kSecurityNone, true, true, &error);
  EXPECT_EQ(1, GetServices().size());
  Mock::VerifyAndClearExpectations(&manager_);

  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiFiServiceRefPtr service1 =
      CreateTemporaryService(kSSID.c_str(), flimflam::kModeManaged,
                             flimflam::kSecurityNone, true, true, &error);

  // Test that a new service was created, but not registered with the
  // manager or added to the provider's service list.
  EXPECT_EQ(1, GetServices().size());
  EXPECT_TRUE(service0 != service1);
  EXPECT_TRUE(service1->HasOneRef());
}

TEST_F(WiFiProviderTest, FindServiceWPA) {
  const string kSSID("an_ssid");
  Error error;
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  WiFiServiceRefPtr service = GetService(
      kSSID.c_str(), flimflam::kModeManaged, flimflam::kSecurityRsn,
       false, true, &error);
  ASSERT_TRUE(service);
  const vector<uint8_t> ssid_bytes(kSSID.begin(), kSSID.end());
  WiFiServiceRefPtr wpa_service(FindService(ssid_bytes, flimflam::kModeManaged,
                                            flimflam::kSecurityWpa));
  EXPECT_TRUE(wpa_service);
  EXPECT_EQ(service.get(), wpa_service.get());
  WiFiServiceRefPtr rsn_service(FindService(ssid_bytes, flimflam::kModeManaged,
                                            flimflam::kSecurityRsn));
  EXPECT_TRUE(rsn_service.get());
  EXPECT_EQ(service.get(), rsn_service.get());
  WiFiServiceRefPtr psk_service(FindService(ssid_bytes, flimflam::kModeManaged,
                                            flimflam::kSecurityPsk));
  EXPECT_EQ(service.get(), psk_service.get());
  WiFiServiceRefPtr wep_service(FindService(ssid_bytes, flimflam::kModeManaged,
                                            flimflam::kSecurityWep));
  EXPECT_TRUE(service.get() != wep_service.get());
  EXPECT_EQ(NULL, wep_service.get());
}

TEST_F(WiFiProviderTest, FindServiceForEndpoint) {
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  Error error;
  const string kSSID("an_ssid");
  WiFiServiceRefPtr service = GetService(
      kSSID.c_str(), flimflam::kModeManaged, flimflam::kSecurityNone,
       false, true, &error);
  ASSERT_TRUE(service);
  WiFiEndpointRefPtr endpoint = MakeEndpoint(kSSID, "00:00:00:00:00:00", 0, 0);
  WiFiServiceRefPtr endpoint_service =
      provider_.FindServiceForEndpoint(endpoint);
  EXPECT_EQ(service.get(), endpoint_service.get());
}

TEST_F(WiFiProviderTest, OnEndpointAdded) {
  provider_.Start();
  const string ssid0("an_ssid");
  const vector<uint8_t> ssid0_bytes(ssid0.begin(), ssid0.end());
  EXPECT_FALSE(FindService(ssid0_bytes, flimflam::kModeManaged,
                           flimflam::kSecurityNone));
  WiFiEndpointRefPtr endpoint0 = MakeEndpoint(ssid0, "00:00:00:00:00:00", 0, 0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_CALL(manager_, UpdateService(_)).Times(1);
  provider_.OnEndpointAdded(endpoint0);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_EQ(1, GetServices().size());
  WiFiServiceRefPtr service0(FindService(ssid0_bytes, flimflam::kModeManaged,
                                         flimflam::kSecurityNone));
  EXPECT_TRUE(service0);
  EXPECT_TRUE(service0->HasEndpoints());

  WiFiEndpointRefPtr endpoint1 = MakeEndpoint(ssid0, "00:00:00:00:00:01", 0, 0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, UpdateService(RefPtrMatch(service0))).Times(1);
  provider_.OnEndpointAdded(endpoint1);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_EQ(1, GetServices().size());

  const string ssid1("another_ssid");
  const vector<uint8_t> ssid1_bytes(ssid1.begin(), ssid1.end());
  EXPECT_FALSE(FindService(ssid1_bytes, flimflam::kModeManaged,
                           flimflam::kSecurityNone));
  WiFiEndpointRefPtr endpoint2 = MakeEndpoint(ssid1, "00:00:00:00:00:02",
                                              0, 0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_CALL(manager_, UpdateService(_)).Times(1);
  provider_.OnEndpointAdded(endpoint2);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_EQ(2, GetServices().size());

  WiFiServiceRefPtr service1(FindService(ssid1_bytes, flimflam::kModeManaged,
                                         flimflam::kSecurityNone));
  EXPECT_TRUE(service1);
  EXPECT_TRUE(service1->HasEndpoints());
  EXPECT_TRUE(service1 != service0);
}

TEST_F(WiFiProviderTest, OnEndpointAddedWithSecurity) {
  provider_.Start();
  const string ssid0("an_ssid");
  const vector<uint8_t> ssid0_bytes(ssid0.begin(), ssid0.end());
  EXPECT_FALSE(FindService(ssid0_bytes, flimflam::kModeManaged,
                           flimflam::kSecurityNone));
  WiFiEndpointRefPtr endpoint0 = MakeEndpoint(ssid0, "00:00:00:00:00:00", 0, 0);
  endpoint0->set_security_mode(flimflam::kSecurityRsn);
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_CALL(manager_, UpdateService(_)).Times(1);
  provider_.OnEndpointAdded(endpoint0);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_EQ(1, GetServices().size());
  WiFiServiceRefPtr service0(FindService(ssid0_bytes, flimflam::kModeManaged,
                                         flimflam::kSecurityWpa));
  EXPECT_TRUE(service0);
  EXPECT_TRUE(service0->HasEndpoints());
  EXPECT_EQ(flimflam::kSecurityPsk, service0->security_);

  WiFiEndpointRefPtr endpoint1 = MakeEndpoint(ssid0, "00:00:00:00:00:01", 0, 0);
  endpoint1->set_security_mode(flimflam::kSecurityWpa);
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, UpdateService(RefPtrMatch(service0))).Times(1);
  provider_.OnEndpointAdded(endpoint1);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_EQ(1, GetServices().size());

  const string ssid1("another_ssid");
  const vector<uint8_t> ssid1_bytes(ssid1.begin(), ssid1.end());
  EXPECT_FALSE(FindService(ssid1_bytes, flimflam::kModeManaged,
                           flimflam::kSecurityNone));
  WiFiEndpointRefPtr endpoint2 = MakeEndpoint(ssid1, "00:00:00:00:00:02", 0, 0);
  endpoint2->set_security_mode(flimflam::kSecurityWpa);
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_CALL(manager_, UpdateService(_)).Times(1);
  provider_.OnEndpointAdded(endpoint2);
  Mock::VerifyAndClearExpectations(&manager_);
  EXPECT_EQ(2, GetServices().size());

  WiFiServiceRefPtr service1(FindService(ssid1_bytes, flimflam::kModeManaged,
                                         flimflam::kSecurityRsn));
  EXPECT_TRUE(service1);
  EXPECT_TRUE(service1->HasEndpoints());
  EXPECT_EQ(flimflam::kSecurityPsk, service1->security_);
  EXPECT_TRUE(service1 != service0);
}

TEST_F(WiFiProviderTest, OnEndpointAddedWhileStopped) {
  // If we don't call provider_.Start(), OnEndpointAdded should have no effect.
  const string ssid("an_ssid");
  WiFiEndpointRefPtr endpoint = MakeEndpoint(ssid, "00:00:00:00:00:00", 0, 0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, UpdateService(_)).Times(0);
  provider_.OnEndpointAdded(endpoint);
  EXPECT_TRUE(GetServices().empty());
}

TEST_F(WiFiProviderTest, OnEndpointAddedToMockService) {
  // The previous test allowed the provider to create its own "real"
  // WiFiServices, which hides some of what we can test with mock
  // services.  Re-do an add-endpoint operation by seeding the provider
  // with a mock service.
  provider_.Start();
  const string ssid0("an_ssid");
  const vector<uint8_t> ssid0_bytes(ssid0.begin(), ssid0.end());
  MockWiFiServiceRefPtr service0 = AddMockService(ssid0_bytes,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  const string ssid1("another_ssid");
  const vector<uint8_t> ssid1_bytes(ssid1.begin(), ssid1.end());
  MockWiFiServiceRefPtr service1 = AddMockService(ssid1_bytes,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  EXPECT_EQ(service0.get(), FindService(ssid0_bytes,
                                        flimflam::kModeManaged,
                                        flimflam::kSecurityNone).get());
  WiFiEndpointRefPtr endpoint0 = MakeEndpoint(ssid0, "00:00:00:00:00:00", 0, 0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, UpdateService(RefPtrMatch(service0))).Times(1);
  EXPECT_CALL(*service0, AddEndpoint(RefPtrMatch(endpoint0))).Times(1);
  EXPECT_CALL(*service1, AddEndpoint(_)).Times(0);
  provider_.OnEndpointAdded(endpoint0);
  Mock::VerifyAndClearExpectations(&manager_);
  Mock::VerifyAndClearExpectations(service0);
  Mock::VerifyAndClearExpectations(service1);

  WiFiEndpointRefPtr endpoint1 = MakeEndpoint(ssid0, "00:00:00:00:00:01", 0, 0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, UpdateService(RefPtrMatch(service0))).Times(1);
  EXPECT_CALL(*service0, AddEndpoint(RefPtrMatch(endpoint1))).Times(1);
  EXPECT_CALL(*service1, AddEndpoint(_)).Times(0);
  provider_.OnEndpointAdded(endpoint1);
  Mock::VerifyAndClearExpectations(&manager_);
  Mock::VerifyAndClearExpectations(service0);
  Mock::VerifyAndClearExpectations(service1);

  WiFiEndpointRefPtr endpoint2 = MakeEndpoint(ssid1, "00:00:00:00:00:02", 0, 0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, UpdateService(RefPtrMatch(service1))).Times(1);
  EXPECT_CALL(*service0, AddEndpoint(_)).Times(0);
  EXPECT_CALL(*service1, AddEndpoint(RefPtrMatch(endpoint2))).Times(1);
  provider_.OnEndpointAdded(endpoint2);
}

TEST_F(WiFiProviderTest, OnEndpointRemoved) {
  provider_.Start();
  const string ssid0("an_ssid");
  const vector<uint8_t> ssid0_bytes(ssid0.begin(), ssid0.end());
  MockWiFiServiceRefPtr service0 = AddMockService(ssid0_bytes,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  const string ssid1("another_ssid");
  const vector<uint8_t> ssid1_bytes(ssid1.begin(), ssid1.end());
  MockWiFiServiceRefPtr service1 = AddMockService(ssid1_bytes,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  EXPECT_EQ(2, GetServices().size());

  // Remove the last endpoint of a non-remembered service.
  WiFiEndpointRefPtr endpoint0 = MakeEndpoint(ssid0, "00:00:00:00:00:00", 0, 0);
  EXPECT_CALL(*service0, RemoveEndpoint(RefPtrMatch(endpoint0))).Times(1);
  EXPECT_CALL(*service1, RemoveEndpoint(_)).Times(0);
  EXPECT_CALL(*service0, HasEndpoints()).WillOnce(Return(false));
  EXPECT_CALL(*service0, IsRemembered()).WillOnce(Return(false));
  EXPECT_CALL(*service0, ResetWiFi()).Times(1);
  EXPECT_CALL(manager_, UpdateService(RefPtrMatch(service0))).Times(0);
  EXPECT_CALL(manager_, DeregisterService(RefPtrMatch(service0))).Times(1);
  provider_.OnEndpointRemoved(endpoint0);
  // Verify now, so it's clear that this happened as a result of the call
  // above, and not anything in the destructor(s).
  Mock::VerifyAndClearExpectations(&manager_);
  Mock::VerifyAndClearExpectations(service0);
  Mock::VerifyAndClearExpectations(service1);
  EXPECT_EQ(1, GetServices().size());
  EXPECT_EQ(service1.get(), GetServices().front().get());
}

TEST_F(WiFiProviderTest, OnEndpointRemovedButHasEndpoints) {
  provider_.Start();
  const string ssid0("an_ssid");
  const vector<uint8_t> ssid0_bytes(ssid0.begin(), ssid0.end());
  MockWiFiServiceRefPtr service0 = AddMockService(ssid0_bytes,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  EXPECT_EQ(1, GetServices().size());

  // Remove an endpoint of a non-remembered service.
  WiFiEndpointRefPtr endpoint0 = MakeEndpoint(ssid0, "00:00:00:00:00:00", 0, 0);
  EXPECT_CALL(*service0, RemoveEndpoint(RefPtrMatch(endpoint0))).Times(1);
  EXPECT_CALL(*service0, HasEndpoints()).WillOnce(Return(true));
  EXPECT_CALL(*service0, IsRemembered()).WillRepeatedly(Return(false));
  EXPECT_CALL(manager_, UpdateService(RefPtrMatch(service0))).Times(1);
  EXPECT_CALL(*service0, ResetWiFi()).Times(0);
  EXPECT_CALL(manager_, DeregisterService(_)).Times(0);
  provider_.OnEndpointRemoved(endpoint0);
  // Verify now, so it's clear that this happened as a result of the call
  // above, and not anything in the destructor(s).
  Mock::VerifyAndClearExpectations(&manager_);
  Mock::VerifyAndClearExpectations(service0);
  EXPECT_EQ(1, GetServices().size());
}

TEST_F(WiFiProviderTest, OnEndpointRemovedButIsRemembered) {
  provider_.Start();
  const string ssid0("an_ssid");
  const vector<uint8_t> ssid0_bytes(ssid0.begin(), ssid0.end());
  MockWiFiServiceRefPtr service0 = AddMockService(ssid0_bytes,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  EXPECT_EQ(1, GetServices().size());

  // Remove the last endpoint of a remembered service.
  WiFiEndpointRefPtr endpoint0 = MakeEndpoint(ssid0, "00:00:00:00:00:00", 0, 0);
  EXPECT_CALL(*service0, RemoveEndpoint(RefPtrMatch(endpoint0))).Times(1);
  EXPECT_CALL(*service0, HasEndpoints()).WillRepeatedly(Return(false));
  EXPECT_CALL(*service0, IsRemembered()).WillOnce(Return(true));
  EXPECT_CALL(manager_, UpdateService(RefPtrMatch(service0))).Times(1);
  EXPECT_CALL(*service0, ResetWiFi()).Times(0);
  EXPECT_CALL(manager_, DeregisterService(_)).Times(0);
  provider_.OnEndpointRemoved(endpoint0);
  // Verify now, so it's clear that this happened as a result of the call
  // above, and not anything in the destructor(s).
  Mock::VerifyAndClearExpectations(&manager_);
  Mock::VerifyAndClearExpectations(service0);
  EXPECT_EQ(1, GetServices().size());
}

TEST_F(WiFiProviderTest, OnEndpointRemovedWhileStopped) {
  // If we don't call provider_.Start(), OnEndpointRemoved should not
  // cause a crash even if a service matching the endpoint does not exist.
  const string ssid("an_ssid");
  WiFiEndpointRefPtr endpoint = MakeEndpoint(ssid, "00:00:00:00:00:00", 0, 0);
  provider_.OnEndpointRemoved(endpoint);
}

TEST_F(WiFiProviderTest, OnServiceUnloaded) {
  // This function should never unregister services itself -- the Manager
  // will automatically deregister the service if OnServiceUnloaded()
  // returns true (via WiFiService::Unload()).
  EXPECT_CALL(manager_, DeregisterService(_)).Times(0);

  MockWiFiServiceRefPtr service = AddMockService(vector<uint8_t>(1, '0'),
                                                 flimflam::kModeManaged,
                                                 flimflam::kSecurityNone,
                                                 false);
  EXPECT_EQ(1, GetServices().size());
  EXPECT_CALL(*service, HasEndpoints()).WillOnce(Return(true));
  EXPECT_CALL(*service, ResetWiFi()).Times(0);
  EXPECT_FALSE(provider_.OnServiceUnloaded(service));
  EXPECT_EQ(1, GetServices().size());
  Mock::VerifyAndClearExpectations(service);

  EXPECT_CALL(*service, HasEndpoints()).WillOnce(Return(false));
  EXPECT_CALL(*service, ResetWiFi()).Times(1);
  EXPECT_TRUE(provider_.OnServiceUnloaded(service));
  // Verify now, so it's clear that this happened as a result of the call
  // above, and not anything in the destructor(s).
  Mock::VerifyAndClearExpectations(service);
  EXPECT_TRUE(GetServices().empty());

  Mock::VerifyAndClearExpectations(&manager_);
}

TEST_F(WiFiProviderTest, FixupServiceEntries) {
  // We test FixupServiceEntries indirectly since it calls a static method
  // in WiFiService.
  EXPECT_CALL(metrics_, SendEnumToUMA(
      "Network.Shill.Wifi.ServiceFixupEntries",
      Metrics::kMetricServiceFixupDefaultProfile,
      Metrics::kMetricServiceFixupMax)).Times(1);
  EXPECT_CALL(storage_, Flush()).Times(1);
  const string kGroupId =
      StringPrintf("%s_0_0_%s_%s",
                   flimflam::kTypeWifi,
                   flimflam::kModeManaged,
                   flimflam::kSecurityNone);
  EXPECT_CALL(storage_,
              GetString(kGroupId, _, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(storage_,
              SetString(kGroupId, _, _)).WillRepeatedly(Return(true));
  set<string> groups;
  groups.insert(kGroupId);
  EXPECT_CALL(storage_, GetGroups()).WillRepeatedly(Return(groups));
  FixupServiceEntries(true);
  Mock::VerifyAndClearExpectations(&metrics_);

  EXPECT_CALL(metrics_, SendEnumToUMA(
      "Network.Shill.Wifi.ServiceFixupEntries",
      Metrics::kMetricServiceFixupUserProfile,
      Metrics::kMetricServiceFixupMax)).Times(1);
  EXPECT_CALL(storage_, Flush()).Times(1);
  FixupServiceEntries(false);
}

TEST_F(WiFiProviderTest, FixupServiceEntriesNothingToDo) {
  EXPECT_CALL(metrics_, SendEnumToUMA(_, _, _)).Times(0);
  EXPECT_CALL(storage_, Flush()).Times(0);
  const string kGroupId =
      StringPrintf("%s_0_0_%s_%s",
                   flimflam::kTypeWifi,
                   flimflam::kModeManaged,
                   flimflam::kSecurityNone);
  EXPECT_CALL(storage_,
              GetString(kGroupId, _, _)).WillRepeatedly(Return(true));
  set<string> groups;
  groups.insert(kGroupId);
  EXPECT_CALL(storage_, GetGroups()).WillOnce(Return(groups));
  FixupServiceEntries(true);
}

TEST_F(WiFiProviderTest, GetHiddenSSIDList) {
  EXPECT_TRUE(provider_.GetHiddenSSIDList().empty());
  const vector<uint8_t> ssid0(1, '0');
  AddMockService(ssid0, flimflam::kModeManaged,
                 flimflam::kSecurityNone, false);
  EXPECT_TRUE(provider_.GetHiddenSSIDList().empty());

  const vector<uint8_t> ssid1(1, '1');
  MockWiFiServiceRefPtr service1 = AddMockService(ssid1,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  true);
  EXPECT_CALL(*service1, IsRemembered()).WillRepeatedly(Return(false));
  EXPECT_TRUE(provider_.GetHiddenSSIDList().empty());

  const vector<uint8_t> ssid2(1, '2');
  MockWiFiServiceRefPtr service2 = AddMockService(ssid2,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  true);
  EXPECT_CALL(*service2, IsRemembered()).WillRepeatedly(Return(true));
  ByteArrays ssid_list = provider_.GetHiddenSSIDList();

  EXPECT_EQ(1, ssid_list.size());
  EXPECT_TRUE(ssid_list[0] == ssid2);

  const vector<uint8_t> ssid3(1, '3');
  MockWiFiServiceRefPtr service3 = AddMockService(ssid3,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  false);
  EXPECT_CALL(*service2, IsRemembered()).WillRepeatedly(Return(true));

  ssid_list = provider_.GetHiddenSSIDList();
  EXPECT_EQ(1, ssid_list.size());
  EXPECT_TRUE(ssid_list[0] == ssid2);

  const vector<uint8_t> ssid4(1, '4');
  MockWiFiServiceRefPtr service4 = AddMockService(ssid4,
                                                  flimflam::kModeManaged,
                                                  flimflam::kSecurityNone,
                                                  true);
  EXPECT_CALL(*service4, IsRemembered()).WillRepeatedly(Return(true));

  ssid_list = provider_.GetHiddenSSIDList();
  EXPECT_EQ(2, ssid_list.size());
  EXPECT_TRUE(ssid_list[0] == ssid2);
  EXPECT_TRUE(ssid_list[1] == ssid4);
}

}  // namespace shill
