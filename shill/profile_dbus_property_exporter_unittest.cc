// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile_dbus_property_exporter.h"

#include <string>

#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/mock_store.h"
#include "shill/service.h"
#include "shill/wifi_service.h"

using std::string;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrNe;
using testing::StrictMock;

namespace shill {

class ProfileDBusPropertyExporterTest : public testing::Test {
 public:
  ProfileDBusPropertyExporterTest() {}

  virtual void SetUp() {
    SetUpWithEntryName("entry_name");
  }

  void SetUpWithEntryName(const string &entry_name) {
    entry_name_ = entry_name;
    storage_.reset(new StrictMock<MockStore>());
    EXPECT_CALL(*storage_.get(), ContainsGroup(StrEq(entry_name_)))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*storage_.get(), ContainsGroup(StrNe(entry_name_)))
        .WillRepeatedly(Return(false));
    exporter_.reset(new ProfileDBusPropertyExporter(storage_.get(),
                                                    entry_name_));
  }

  void ExpectBoolProperty(const string &key, bool value) {
    EXPECT_CALL(*storage_.get(), GetBool(StrEq(entry_name_), StrEq(key), _))
        .WillOnce(DoAll(SetArgumentPointee<2>(value), Return(true)));
  }

  void ExpectStringProperty(const string &key, const string &value) {
    EXPECT_CALL(*storage_.get(), GetString(StrEq(entry_name_), StrEq(key), _))
        .WillOnce(DoAll(SetArgumentPointee<2>(value), Return(true)));
  }

  void ExpectUnknownProperties() {
    EXPECT_CALL(*storage_.get(), GetBool(StrEq(entry_name_), _, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*storage_.get(), GetString(StrEq(entry_name_), _, _))
        .WillRepeatedly(Return(false));
  }

  bool GetBoolProperty(
      ProfileDBusPropertyExporter::PropertyList *props, const string key) {
    return (*props)[key].reader().get_bool();
  }

  string GetStringProperty(
      ProfileDBusPropertyExporter::PropertyList *props, const string key) {
    return (*props)[key].reader().get_string();
  }

 protected:
  string entry_name_;
  scoped_ptr<MockStore> storage_;
  scoped_ptr<ProfileDBusPropertyExporter> exporter_;
};

TEST_F(ProfileDBusPropertyExporterTest, LoadWrongGroup) {
  ProfileDBusPropertyExporter exporter(storage_.get(), "not_entry_name");
  ProfileDBusPropertyExporter::PropertyList props;
  Error e;
  EXPECT_FALSE(exporter.LoadServiceProperties(&props, &e));
  EXPECT_EQ(Error::kNotFound, e.type());
}

TEST_F(ProfileDBusPropertyExporterTest, InvalidTechnology) {
  SetUpWithEntryName("unknown_technology");
  ProfileDBusPropertyExporter::PropertyList props;
  Error e;
  EXPECT_FALSE(exporter_->LoadServiceProperties(&props, &e));
  EXPECT_EQ(Error::kInternalError, e.type());
}

TEST_F(ProfileDBusPropertyExporterTest, MinimalProperties) {
  SetUpWithEntryName("ethernet_service");
  ExpectUnknownProperties();
  ProfileDBusPropertyExporter::PropertyList props;
  Error e;
  EXPECT_TRUE(exporter_->LoadServiceProperties(&props, &e));
  EXPECT_EQ(1, props.size());
  EXPECT_EQ(flimflam::kTypeEthernet,
            GetStringProperty(&props, flimflam::kTypeProperty));
}

TEST_F(ProfileDBusPropertyExporterTest, OverrideTypeProperty) {
  SetUpWithEntryName("ethernet_service");
  const string service_type = "special_type";
  ExpectUnknownProperties();
  ExpectStringProperty(Service::kStorageType, service_type);
  ProfileDBusPropertyExporter::PropertyList props;
  Error e;
  EXPECT_TRUE(exporter_->LoadServiceProperties(&props, &e));
  EXPECT_EQ(1, props.size());
  EXPECT_EQ(service_type, GetStringProperty(&props, flimflam::kTypeProperty));
}

TEST_F(ProfileDBusPropertyExporterTest, AllServiceProperties) {
  SetUpWithEntryName("ethernet_service");
  const bool auto_connect(true);
  ExpectBoolProperty(Service::kStorageAutoConnect, auto_connect);
  const string error("no carrier");
  ExpectStringProperty(Service::kStorageError, error);
  const string guid("guid");
  ExpectStringProperty(Service::kStorageGUID, guid);
  const string name("fastnet");
  ExpectStringProperty(Service::kStorageName, name);
  const string type("100baset");
  ExpectStringProperty(Service::kStorageType, type);
  const string uidata("ui data");
  ExpectStringProperty(Service::kStorageUIData, uidata);

  ProfileDBusPropertyExporter::PropertyList props;
  Error e;
  EXPECT_TRUE(exporter_->LoadServiceProperties(&props, &e));
  EXPECT_EQ(auto_connect, GetBoolProperty(&props,
                                          flimflam::kAutoConnectProperty));
  EXPECT_EQ(error, GetStringProperty(&props, flimflam::kErrorProperty));
  EXPECT_EQ(guid, GetStringProperty(&props, flimflam::kGuidProperty));
  EXPECT_EQ(name, GetStringProperty(&props, flimflam::kNameProperty));
  EXPECT_EQ(type, GetStringProperty(&props, flimflam::kTypeProperty));
  EXPECT_EQ(uidata, GetStringProperty(&props, flimflam::kUIDataProperty));
}

TEST_F(ProfileDBusPropertyExporterTest, MinimalWiFiServiceProperties) {
  SetUpWithEntryName("wifi_address_ssid_superfly_unbreakable_crypto");
  ExpectUnknownProperties();
  ProfileDBusPropertyExporter::PropertyList props;
  Error e;
  EXPECT_TRUE(exporter_->LoadServiceProperties(&props, &e));
  EXPECT_EQ("superfly", GetStringProperty(&props, flimflam::kModeProperty));
  EXPECT_EQ("unbreakable_crypto",
            GetStringProperty(&props, flimflam::kSecurityProperty));
}

TEST_F(ProfileDBusPropertyExporterTest, AllWiFiServiceProperties) {
  SetUpWithEntryName("wifi_service");
  ExpectUnknownProperties();
  const bool hidden_ssid(true);
  ExpectBoolProperty(WiFiService::kStorageHiddenSSID, hidden_ssid);
  const string mode("superfly");
  ExpectStringProperty(WiFiService::kStorageMode, mode);
  const string security("unbreakablecrypto");
  ExpectStringProperty(WiFiService::kStorageSecurity, security);

  ProfileDBusPropertyExporter::PropertyList props;
  Error e;
  EXPECT_TRUE(exporter_->LoadServiceProperties(&props, &e));
  EXPECT_EQ(hidden_ssid, GetBoolProperty(&props, flimflam::kWifiHiddenSsid));
  EXPECT_EQ(mode, GetStringProperty(&props, flimflam::kModeProperty));
  EXPECT_EQ(security, GetStringProperty(&props, flimflam::kSecurityProperty));
}

}  // namespace shill
