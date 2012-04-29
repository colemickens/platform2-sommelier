// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service.h"

#include <map>
#include <string>
#include <vector>

#include <base/bind.h>
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
#include "shill/mock_connection.h"
#include "shill/mock_device_info.h"
#include "shill/mock_manager.h"
#include "shill/mock_profile.h"
#include "shill/mock_store.h"
#include "shill/property_store_inspector.h"
#include "shill/property_store_unittest.h"
#include "shill/service_under_test.h"

using base::Bind;
using base::Unretained;
using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
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

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:

  MockManager mock_manager_;
  scoped_refptr<ServiceUnderTest> service_;
  string storage_id_;
};

TEST_F(ServiceTest, Constructor) {
  EXPECT_TRUE(service_->save_credentials_);
  EXPECT_EQ(Service::kCheckPortalAuto, service_->check_portal_);
  EXPECT_EQ(Service::kStateIdle, service_->state());
}

TEST_F(ServiceTest, GetProperties) {
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    string expected("true");
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
    EXPECT_EQ(props[flimflam::kDeviceProperty].reader().get_path(),
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
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                          flimflam::kAutoConnectProperty,
                                          PropertyStoreTest::kBoolV,
                                          &error));
  }
  // Ensure that we can perform a trivial set of the Name property (to its
  // current value) but an attempt to set the property to a different value
  // fails.
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         flimflam::kNameProperty,
                                         DBusAdaptor::StringToVariant(
                                             service_->friendly_name()),
                                         &error));
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                          flimflam::kNameProperty,
                                          PropertyStoreTest::kStringV,
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
  EXPECT_EQ(Service::kStateIdle, service_->state());
  EXPECT_EQ(Service::kFailureUnknown, service_->failure());

  ServiceRefPtr service_ref(service_);

  EXPECT_CALL(*dynamic_cast<ServiceMockAdaptor *>(service_->adaptor_.get()),
              EmitStringChanged(flimflam::kStateProperty, _)).Times(5);
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
  EXPECT_TRUE(service_->IsFailed());
  EXPECT_GT(service_->failed_time_, 0);
  EXPECT_EQ(Service::kStateFailure, service_->state());
  EXPECT_EQ(Service::kFailureOutOfRange, service_->failure());

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetState(Service::kStateConnected);
  EXPECT_FALSE(service_->IsFailed());
  EXPECT_EQ(service_->failed_time_, 0);

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetFailureSilent(Service::kFailurePinMissing);
  EXPECT_TRUE(service_->IsFailed());
  EXPECT_GT(service_->failed_time_, 0);
  EXPECT_EQ(Service::kStateIdle, service_->state());
  EXPECT_EQ(Service::kFailurePinMissing, service_->failure());
}

TEST_F(ServiceTest, ActivateCellularModem) {
  ResultCallback callback =
      Bind(&ServiceTest::TestCallback, Unretained(this));
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  Error error;
  service_->ActivateCellularModem("Carrier", &error, callback);
  EXPECT_TRUE(error.IsFailure());
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
  const char *reason;
  service_->set_connectable(true);
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // We should not auto-connect to a Service that a user has
  // deliberately disconnected.
  Error error;
  service_->Disconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnExplicitDisconnect, reason);

  // But if the Service is reloaded, it is eligible for auto-connect
  // again.
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(true));
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // A deliberate Connect should also re-enable auto-connect.
  service_->Disconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  service_->Connect(&error);
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // TODO(quiche): After we have resume handling in place, test that
  // we re-enable auto-connect on resume. crosbug.com/25213

  service_->SetState(Service::kStateConnected);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnConnected, reason);

  service_->SetState(Service::kStateAssociating);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnConnecting, reason);
}

TEST_F(ServiceTest, ConfigureBadProperty) {
  KeyValueStore args;
  args.SetString("XXXInvalid", "Value");
  Error error;
  service_->Configure(args, &error);
  EXPECT_FALSE(error.IsSuccess());
}

TEST_F(ServiceTest, ConfigureBoolProperty) {
  service_->MakeFavorite();
  service_->set_auto_connect(false);
  ASSERT_FALSE(service_->auto_connect());
  KeyValueStore args;
  args.SetBool(flimflam::kAutoConnectProperty, true);
  Error error;
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(service_->auto_connect());
}

TEST_F(ServiceTest, ConfigureStringProperty) {
  const string kEAPManagement0 = "management_zero";
  const string kEAPManagement1 = "management_one";
  service_->SetEAPKeyManagement(kEAPManagement0);
  ASSERT_EQ(kEAPManagement0, service_->GetEAPKeyManagement());
  KeyValueStore args;
  args.SetString(flimflam::kEapKeyMgmtProperty, kEAPManagement1);
  Error error;
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kEAPManagement1, service_->GetEAPKeyManagement());
}

TEST_F(ServiceTest, ConfigureIgnoredProperty) {
  service_->MakeFavorite();
  service_->set_auto_connect(false);
  ASSERT_FALSE(service_->auto_connect());
  KeyValueStore args;
  args.SetBool(flimflam::kAutoConnectProperty, true);
  Error error;
  service_->IgnoreParameterForConfigure(flimflam::kAutoConnectProperty);
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_FALSE(service_->auto_connect());
}

TEST_F(ServiceTest, IsRemembered) {
  ServiceConstRefPtr service_ref(service_);
  service_->set_profile(NULL);
  EXPECT_CALL(mock_manager_, IsServiceEphemeral(_)).Times(0);
  EXPECT_FALSE(service_->IsRemembered());

  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), manager()));
  service_->set_profile(profile);
  EXPECT_CALL(mock_manager_, IsServiceEphemeral(service_ref))
     .WillOnce(Return(true))
     .WillOnce(Return(false));
  EXPECT_FALSE(service_->IsRemembered());
  EXPECT_TRUE(service_->IsRemembered());
}

TEST_F(ServiceTest, OnPropertyChanged) {
  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), manager()));
  service_->set_profile(NULL);
  // Expect no crash.
  service_->OnPropertyChanged("");

  // Expect no call to Update if the profile has no storage.
  service_->set_profile(profile);
  EXPECT_CALL(*profile, UpdateService(_)).Times(0);
  EXPECT_CALL(*profile, GetConstStorage())
      .WillOnce(Return(reinterpret_cast<StoreInterface *>(NULL)));
  service_->OnPropertyChanged("");

  // Expect call to Update if the profile has storage.
  EXPECT_CALL(*profile, UpdateService(_)).Times(1);
  NiceMock<MockStore> storage;
  EXPECT_CALL(*profile, GetConstStorage()).WillOnce(Return(&storage));
  service_->OnPropertyChanged("");
}


TEST_F(ServiceTest, RecheckPortal) {
  ServiceRefPtr service_ref(service_);
  service_->state_ = Service::kStateIdle;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(_)).Times(0);
  service_->OnPropertyChanged(flimflam::kCheckPortalProperty);

  service_->state_ = Service::kStatePortal;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(service_ref)).Times(1);
  service_->OnPropertyChanged(flimflam::kCheckPortalProperty);

  service_->state_ = Service::kStateConnected;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(service_ref)).Times(1);
  service_->OnPropertyChanged(flimflam::kProxyConfigProperty);

  service_->state_ = Service::kStateOnline;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(service_ref)).Times(1);
  service_->OnPropertyChanged(flimflam::kCheckPortalProperty);

  service_->state_ = Service::kStatePortal;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(_)).Times(0);
  service_->OnPropertyChanged(flimflam::kEAPKeyIDProperty);
}

TEST_F(ServiceTest, SetCheckPortal) {
  ServiceRefPtr service_ref(service_);
  {
    Error error;
    service_->SetCheckPortal("false", &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(Service::kCheckPortalFalse, service_->check_portal_);
  }
  {
    Error error;
    service_->SetCheckPortal("true", &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(Service::kCheckPortalTrue, service_->check_portal_);
  }
  {
    Error error;
    service_->SetCheckPortal("auto", &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(Service::kCheckPortalAuto, service_->check_portal_);
  }
  {
    Error error;
    service_->SetCheckPortal("xxx", &error);
    EXPECT_FALSE(error.IsSuccess());
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ(Service::kCheckPortalAuto, service_->check_portal_);
  }
}

// Make sure a property is registered as a write only property
// by reading and comparing all string properties returned on the store.
// Subtle: We need to convert the test argument back and forth between
// string and ::DBus::Variant because this is the parameter type that
// our supeclass (PropertyStoreTest) is declared with.
class ReadOnlyServicePropertyTest : public ServiceTest {};
TEST_P(ReadOnlyServicePropertyTest, PropertyWriteOnly) {
  string property(GetParam().reader().get_string());
  PropertyStoreInspector inspector(&service_->store());
  EXPECT_FALSE(inspector.ContainsStringProperty(property));
}

INSTANTIATE_TEST_CASE_P(
    ReadOnlyServicePropertyTestInstance,
    ReadOnlyServicePropertyTest,
    Values(
        DBusAdaptor::StringToVariant(flimflam::kEapPrivateKeyPasswordProperty),
        DBusAdaptor::StringToVariant(flimflam::kEapPasswordProperty)));


TEST_F(ServiceTest, GetIPConfigRpcIdentifier) {
  {
    Error error;
    EXPECT_EQ("/", service_->GetIPConfigRpcIdentifier(&error));
    EXPECT_EQ(Error::kNotFound, error.type());
  }

  scoped_ptr<MockDeviceInfo> mock_device_info(
      new NiceMock<MockDeviceInfo>(control_interface(), dispatcher(), metrics(),
                                   &mock_manager_));
  scoped_refptr<MockConnection> mock_connection(
      new NiceMock<MockConnection>(mock_device_info.get()));

  service_->connection_ = mock_connection;

  {
    Error error;
    const string empty_string;
    EXPECT_CALL(*mock_connection, ipconfig_rpc_identifier())
        .WillOnce(ReturnRef(empty_string));
    EXPECT_EQ("/", service_->GetIPConfigRpcIdentifier(&error));
    EXPECT_EQ(Error::kNotFound, error.type());
  }

  {
    Error error;
    const string nonempty_string("/ipconfig/path");
    EXPECT_CALL(*mock_connection, ipconfig_rpc_identifier())
        .WillOnce(ReturnRef(nonempty_string));
    EXPECT_EQ(nonempty_string, service_->GetIPConfigRpcIdentifier(&error));
    EXPECT_EQ(Error::kSuccess, error.type());
  }

  // Assure orderly destruction of the Connection before DeviceInfo.
  service_->connection_ = NULL;
  mock_connection = NULL;
  mock_device_info.reset();
}

}  // namespace shill
