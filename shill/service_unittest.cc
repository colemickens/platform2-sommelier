// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service.h"

#include <map>
#include <string>
#include <vector>

#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/ethernet_service.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_manager.h"
#include "shill/mock_store.h"
#include "shill/property_store_unittest.h"
#include "shill/service_under_test.h"

using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::SetArgumentPointee;
using testing::Test;
using testing::Values;

namespace shill {

class ServiceTest : public PropertyStoreTest {
 public:
  ServiceTest()
      : mock_manager_(control_interface(), dispatcher(), metrics(), glib()),
        service_(new ServiceUnderTest(control_interface(),
                                      dispatcher(),
                                      metrics(),
                                      &mock_manager_)),
        storage_id_(ServiceUnderTest::kStorageId) {
  }

  virtual ~ServiceTest() {}

 protected:
  MockManager mock_manager_;
  scoped_refptr<ServiceUnderTest> service_;
  string storage_id_;
};

TEST_F(ServiceTest, Constructor) {
  EXPECT_TRUE(service_->save_credentials_);
  EXPECT_EQ(Service::kCheckPortalAuto, service_->check_portal_);
}

TEST_F(ServiceTest, GetProperties) {
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    string expected("portal_list");
    service_->mutable_store()->SetStringProperty(flimflam::kCheckPortalProperty,
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
    service_->set_favorite(true);
    service_->mutable_store()->SetBoolProperty(flimflam::kAutoConnectProperty,
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
    service_->mutable_store()->SetInt32Property(flimflam::kPriorityProperty,
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
              string(ServiceUnderTest::kRpcId));
  }
}

TEST_F(ServiceTest, SetProperty) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         flimflam::kSaveCredentialsProperty,
                                         PropertyStoreTest::kBoolV,
                                         &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         flimflam::kPriorityProperty,
                                         PropertyStoreTest::kInt32V,
                                         &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         flimflam::kEAPEAPProperty,
                                         PropertyStoreTest::kStringV,
                                         &error));
  }
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                          flimflam::kFavoriteProperty,
                                          PropertyStoreTest::kBoolV,
                                          &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  {
    ::DBus::Error error;
    service_->set_favorite(true);
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         flimflam::kAutoConnectProperty,
                                         PropertyStoreTest::kBoolV,
                                         &error));
  }
  {
    ::DBus::Error error;
    service_->set_favorite(false);
    EXPECT_FALSE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                          flimflam::kAutoConnectProperty,
                                          PropertyStoreTest::kBoolV,
                                          &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
}

TEST_F(ServiceTest, Load) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(true));
  EXPECT_CALL(storage, GetString(storage_id_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service_->Load(&storage));
}

TEST_F(ServiceTest, LoadFail) {
  StrictMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(false));
  EXPECT_FALSE(service_->Load(&storage));
}

TEST_F(ServiceTest, SaveString) {
  MockStore storage;
  static const char kKey[] = "test-key";
  static const char kData[] = "test-data";
  EXPECT_CALL(storage, SetString(storage_id_, kKey, kData))
      .WillOnce(Return(true));
  service_->SaveString(&storage, storage_id_, kKey, kData, false, true);
}

TEST_F(ServiceTest, SaveStringCrypted) {
  MockStore storage;
  static const char kKey[] = "test-key";
  static const char kData[] = "test-data";
  EXPECT_CALL(storage, SetCryptedString(storage_id_, kKey, kData))
      .WillOnce(Return(true));
  service_->SaveString(&storage, storage_id_, kKey, kData, true, true);
}

TEST_F(ServiceTest, SaveStringDontSave) {
  MockStore storage;
  static const char kKey[] = "test-key";
  EXPECT_CALL(storage, DeleteKey(storage_id_, kKey))
      .WillOnce(Return(true));
  service_->SaveString(&storage, storage_id_, kKey, "data", false, false);
}

TEST_F(ServiceTest, SaveStringEmpty) {
  MockStore storage;
  static const char kKey[] = "test-key";
  EXPECT_CALL(storage, DeleteKey(storage_id_, kKey))
      .WillOnce(Return(true));
  service_->SaveString(&storage, storage_id_, kKey, "", true, true);
}

TEST_F(ServiceTest, Save) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, SetString(storage_id_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(storage, DeleteKey(storage_id_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(service_->Save(&storage));
}

TEST_F(ServiceTest, Unload) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(true));
  static const string string_value("value");
  EXPECT_CALL(storage, GetString(storage_id_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(string_value), Return(true)));
  ASSERT_TRUE(service_->Load(&storage));
  // TODO(pstew): A single string property in the service is tested as
  // a sentinel that properties are being set and reset at the rit times.
  // However, since property load/store is essentially a manual process,
  // it is error prone and should either be exhaustively unit-tested or
  // a generic framework for registering loaded/stored properties should
  // be created. crosbug.com/24859
  EXPECT_EQ(string_value, service_->ui_data_);
  service_->Unload();
  EXPECT_EQ(string(""), service_->ui_data_);
}

TEST_F(ServiceTest, State) {
  EXPECT_EQ(Service::kStateUnknown, service_->state());
  EXPECT_EQ(Service::kFailureUnknown, service_->failure());

  ServiceRefPtr service_ref(service_);

  // TODO(quiche): make this EXPECT_CALL work (crosbug.com/20154)
  // EXPECT_CALL(*dynamic_cast<ServiceMockAdaptor *>(service_->adaptor_.get()),
  //     EmitStringChanged(flimflam::kStateProperty, _));
  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetState(Service::kStateConnected);
  // A second state change shouldn't cause another update
  service_->SetState(Service::kStateConnected);

  EXPECT_EQ(Service::kStateConnected, service_->state());
  EXPECT_EQ(Service::kFailureUnknown, service_->failure());
  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetState(Service::kStateDisconnected);

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetFailure(Service::kFailureOutOfRange);

  EXPECT_EQ(Service::kStateFailure, service_->state());
  EXPECT_EQ(Service::kFailureOutOfRange, service_->failure());
}

TEST_F(ServiceTest, ActivateCellularModem) {
  MockReturner returner;
  EXPECT_CALL(returner, Return()).Times(0);
  EXPECT_CALL(returner, ReturnError(_));
  Error error;
  service_->ActivateCellularModem("Carrier", &returner);
}

TEST_F(ServiceTest, MakeFavorite) {
  EXPECT_FALSE(service_->favorite());
  EXPECT_FALSE(service_->auto_connect());

  service_->MakeFavorite();
  EXPECT_TRUE(service_->favorite());
  EXPECT_TRUE(service_->auto_connect());
}

TEST_F(ServiceTest, ReMakeFavorite) {
  service_->MakeFavorite();
  EXPECT_TRUE(service_->favorite());
  EXPECT_TRUE(service_->auto_connect());

  service_->set_auto_connect(false);
  service_->MakeFavorite();
  EXPECT_TRUE(service_->favorite());
  EXPECT_FALSE(service_->auto_connect());
}

TEST_F(ServiceTest, IsAutoConnectable) {
  service_->set_connectable(true);
  EXPECT_TRUE(service_->IsAutoConnectable());

  // We should not auto-connect to a Service that a user has
  // deliberately disconnected.
  Error error;
  service_->Disconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable());

  // But if the Service is reloaded, it is eligible for auto-connect
  // again.
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(true));
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_TRUE(service_->IsAutoConnectable());

  // A deliberate Connect should also re-enable auto-connect.
  service_->Disconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable());
  service_->Connect(&error);
  EXPECT_TRUE(service_->IsAutoConnectable());

  // TODO(quiche): After we have resume handling in place, test that
  // we re-enable auto-connect on resume. crosbug.com/25213

  service_->SetState(Service::kStateConnected);
  EXPECT_FALSE(service_->IsAutoConnectable());

  service_->SetState(Service::kStateAssociating);
  EXPECT_FALSE(service_->IsAutoConnectable());
}

// Make sure a property is registered as a write only property
// by reading and comparing all string properties returned on the store.
// Subtle: We need to convert the test argument back and forth between
// string and ::DBus::Variant because this is the parameter type that
// our supeclass (PropertyStoreTest) is declared with.
class ReadOnlyServicePropertyTest : public ServiceTest {};
TEST_P(ReadOnlyServicePropertyTest, PropertyWriteOnly) {
  ReadablePropertyConstIterator<string> it =
      (service_->store()).GetStringPropertiesIter();
  string property(GetParam().reader().get_string());
  for( ; !it.AtEnd(); it.Advance())
    EXPECT_NE(it.Key(), property);
}

INSTANTIATE_TEST_CASE_P(
    ReadOnlyServicePropertyTestInstance,
    ReadOnlyServicePropertyTest,
    Values(
        DBusAdaptor::StringToVariant(flimflam::kEapPrivateKeyPasswordProperty),
        DBusAdaptor::StringToVariant(flimflam::kEapPasswordProperty)));

}  // namespace shill
