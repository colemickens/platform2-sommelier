// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service.h"

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/ethernet_service.h"
#include "shill/manager.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/property_store_unittest.h"
#include "shill/shill_event.h"

using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

class ServiceTest : public PropertyStoreTest {
 public:
  static const char kMockServiceName[];
  static const char kMockDeviceRpcId[];
  static const char kProfileName[];

  ServiceTest()
      : service_(new MockService(&control_interface_,
                                 &dispatcher_,
                                 new MockProfile(&control_interface_, &glib_),
                                 kMockServiceName)) {
  }

  virtual ~ServiceTest() {}

 protected:
  scoped_refptr<MockService> service_;
};

const char ServiceTest::kMockServiceName[] = "mock-service";

const char ServiceTest::kMockDeviceRpcId[] = "mock-device-rpc";

const char ServiceTest::kProfileName[] = "profile";

TEST_F(ServiceTest, Constructor) {
  EXPECT_TRUE(service_->save_credentials_);
  EXPECT_EQ(Service::kCheckPortalAuto, service_->check_portal_);
}

TEST_F(ServiceTest, GetProperties) {
  EXPECT_CALL(*service_.get(), CalculateState()).WillRepeatedly(Return(""));
  EXPECT_CALL(*service_.get(), GetDeviceRpcId())
      .WillRepeatedly(Return(ServiceTest::kMockDeviceRpcId));
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    string expected("portal_list");
    service_->store()->SetStringProperty(flimflam::kCheckPortalProperty,
                                         expected,
                                         &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kCheckPortalProperty) == props.end());
    EXPECT_EQ(props[flimflam::kCheckPortalProperty].reader().get_string(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    bool expected = true;
    service_->store()->SetBoolProperty(flimflam::kAutoConnectProperty,
                                       expected,
                                       &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kAutoConnectProperty) == props.end());
    EXPECT_EQ(props[flimflam::kAutoConnectProperty].reader().get_bool(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kConnectableProperty) == props.end());
    EXPECT_EQ(props[flimflam::kConnectableProperty].reader().get_bool(), false);
  }
  {
    ::DBus::Error dbus_error;
    int32 expected = 127;
    service_->store()->SetInt32Property(flimflam::kPriorityProperty,
                                        expected,
                                        &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kPriorityProperty) == props.end());
    EXPECT_EQ(props[flimflam::kPriorityProperty].reader().get_int32(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kDeviceProperty) == props.end());
    EXPECT_EQ(props[flimflam::kDeviceProperty].reader().get_string(),
              string(ServiceTest::kMockDeviceRpcId));
  }
}

TEST_F(ServiceTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_->store(),
                                            flimflam::kSaveCredentialsProperty,
                                            PropertyStoreTest::kBoolV,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_->store(),
                                            flimflam::kPriorityProperty,
                                            PropertyStoreTest::kInt32V,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_->store(),
                                            flimflam::kEAPEAPProperty,
                                            PropertyStoreTest::kStringV,
                                            &error));
  }
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_->store(),
                                             flimflam::kFavoriteProperty,
                                             PropertyStoreTest::kBoolV,
                                             &error));
    EXPECT_EQ(invalid_args_, error.name());
  }
}

TEST_F(ServiceTest, MoveService) {
  // I want to ensure that the Profiles are managing this Service object
  // lifetime properly, so I can't hold a ref to it here.
  ProfileRefPtr profile(new Profile(&control_interface_, &glib_));
  {
    ServiceRefPtr s2(
        new MockService(&control_interface_,
                        &dispatcher_,
                        new MockProfile(&control_interface_, &glib_),
                        kMockServiceName));
    profile->services_[kMockServiceName] = s2;
  }

  // Now, move the service to another profile.
  ProfileRefPtr profile2(new Profile(&control_interface_, &glib_));
  map<string, ServiceRefPtr>::iterator it =
      profile->services_.find(kMockServiceName);
  ASSERT_TRUE(it != profile->services_.end());

  profile2->AdoptService(it->first, it->second);
  // Force destruction of the original Profile, to ensure that the Service
  // is kept alive and populated with data.
  profile = NULL;
  {
    map<string, ServiceRefPtr>::iterator it =
        profile2->services_.find(kMockServiceName);
    Error error(Error::kInvalidProperty, "");
    ::DBus::Error dbus_error;
    map<string, ::DBus::Variant> props;
    bool expected = true;
    it->second->store()->SetBoolProperty(flimflam::kAutoConnectProperty,
                                         expected,
                                         &error);
    DBusAdaptor::GetProperties(it->second->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kAutoConnectProperty) == props.end());
    EXPECT_EQ(props[flimflam::kAutoConnectProperty].reader().get_bool(),
              expected);
  }
}

TEST_F(ServiceTest, GetStorageIdentifier) {
  EXPECT_EQ(kMockServiceName, service_->GetStorageIdentifier());
}

TEST_F(ServiceTest, Load) {
  NiceMock<MockStore> storage;
  const string id = kMockServiceName;
  EXPECT_CALL(storage, ContainsGroup(id)).WillOnce(Return(true));
  EXPECT_CALL(storage, GetString(id, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service_->Load(&storage));
}

TEST_F(ServiceTest, LoadFail) {
  StrictMock<MockStore> storage;
  const string id = kMockServiceName;
  EXPECT_CALL(storage, ContainsGroup(kMockServiceName)).WillOnce(Return(false));
  EXPECT_FALSE(service_->Load(&storage));
}

TEST_F(ServiceTest, SaveString) {
  MockStore storage;
  static const char kKey[] = "test-key";
  static const char kData[] = "test-data";
  EXPECT_CALL(storage, SetString(kMockServiceName, kKey, kData))
      .WillOnce(Return(true));
  service_->SaveString(&storage, kKey, kData, false, true);
}

TEST_F(ServiceTest, SaveStringCrypted) {
  MockStore storage;
  static const char kKey[] = "test-key";
  static const char kData[] = "test-data";
  EXPECT_CALL(storage, SetCryptedString(kMockServiceName, kKey, kData))
      .WillOnce(Return(true));
  service_->SaveString(&storage, kKey, kData, true, true);
}

TEST_F(ServiceTest, SaveStringDontSave) {
  MockStore storage;
  static const char kKey[] = "test-key";
  EXPECT_CALL(storage, DeleteKey(kMockServiceName, kKey))
      .WillOnce(Return(true));
  service_->SaveString(&storage, kKey, "data", false, false);
}

TEST_F(ServiceTest, SaveStringEmpty) {
  MockStore storage;
  static const char kKey[] = "test-key";
  EXPECT_CALL(storage, DeleteKey(kMockServiceName, kKey))
      .WillOnce(Return(true));
  service_->SaveString(&storage, kKey, "", true, true);
}

TEST_F(ServiceTest, Save) {
  NiceMock<MockStore> storage;
  const string id = kMockServiceName;
  EXPECT_CALL(storage, SetString(id, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(storage, DeleteKey(id, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service_->Save(&storage));
}

}  // namespace shill
