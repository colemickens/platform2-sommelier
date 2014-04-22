// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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
#include "shill/mock_diagnostics_reporter.h"
#include "shill/mock_eap_credentials.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_log.h"
#include "shill/mock_manager.h"
#include "shill/mock_power_manager.h"
#include "shill/mock_profile.h"
#include "shill/mock_proxy_factory.h"
#include "shill/mock_store.h"
#include "shill/mock_time.h"
#include "shill/property_store_unittest.h"
#include "shill/service_property_change_test.h"
#include "shill/service_under_test.h"

using base::Bind;
using base::Unretained;
using std::deque;
using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::DefaultValue;
using testing::DoAll;
using testing::HasSubstr;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnNull;
using testing::ReturnRef;
using testing::StrictMock;
using testing::StrNe;
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
        service2_(new ServiceUnderTest(control_interface(),
                                       dispatcher(),
                                       metrics(),
                                       &mock_manager_)),
        storage_id_(ServiceUnderTest::kStorageId),
        power_manager_(new MockPowerManager(NULL, &proxy_factory_)),
        eap_(new MockEapCredentials()) {
    ON_CALL(proxy_factory_, CreatePowerManagerProxy(_))
        .WillByDefault(ReturnNull());

    service_->time_ = &time_;
    DefaultValue<Timestamp>::Set(Timestamp());
    service_->diagnostics_reporter_ = &diagnostics_reporter_;
    service_->eap_.reset(eap_);  // Passes ownership.
    mock_manager_.running_ = true;
    mock_manager_.set_power_manager(power_manager_);  // Passes ownership.
  }

  virtual ~ServiceTest() {}

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  typedef scoped_refptr<MockProfile> MockProfileRefPtr;

  ServiceMockAdaptor *GetAdaptor() {
    return dynamic_cast<ServiceMockAdaptor *>(service_->adaptor());
  }

  string GetFriendlyName() { return service_->friendly_name(); }

  void SetManagerRunning(bool running) { mock_manager_.running_ = running; }

  void SetSuspending(bool suspending) {
    power_manager_->suspending_ = suspending;
  }

  void SetExplicitlyDisconnected(bool explicitly) {
    service_->explicitly_disconnected_ = explicitly;
  }

  void SetStateField(Service::ConnectState state) { service_->state_ = state; }

  Service::ConnectState GetPreviousState() const {
    return service_->previous_state_;
  }

  void NoteDisconnectEvent() {
    service_->NoteDisconnectEvent();
  }

  deque<Timestamp> *GetDisconnects() {
    return &service_->disconnects_;
  }
  deque<Timestamp> *GetMisconnects() {
    return &service_->misconnects_;
  }

  Timestamp GetTimestamp(int monotonic_seconds, const string &wall_clock) {
    struct timeval monotonic = { .tv_sec = monotonic_seconds, .tv_usec = 0 };
    return Timestamp(monotonic, wall_clock);
  }

  void PushTimestamp(deque<Timestamp> *timestamps,
                     int monotonic_seconds,
                     const string &wall_clock) {
    timestamps->push_back(GetTimestamp(monotonic_seconds, wall_clock));
  }

  int GetDisconnectsMonitorSeconds() {
    return Service::kDisconnectsMonitorSeconds;
  }

  int GetMisconnectsMonitorSeconds() {
    return Service::kMisconnectsMonitorSeconds;
  }

  int GetReportDisconnectsThreshold() {
    return Service::kReportDisconnectsThreshold;
  }

  int GetReportMisconnectsThreshold() {
    return Service::kReportMisconnectsThreshold;
  }

  int GetMaxDisconnectEventHistory() {
    return Service::kMaxDisconnectEventHistory;
  }

  static Strings ExtractWallClockToStrings(const deque<Timestamp> &timestamps) {
    return Service::ExtractWallClockToStrings(timestamps);
  }

  bool GetAutoConnect(Error *error) {
    return service_->GetAutoConnect(error);
  }

  void ClearAutoConnect(Error *error) {
    service_->ClearAutoConnect(error);
  }

  bool SetAutoConnectFull(bool connect, Error *error) {
    return service_->SetAutoConnectFull(connect, error);
  }

  int GetConsecutiveDHCPFailures() {
    return service_->consecutive_dhcp_failures_;
  }

  void SetConsecutiveDHCPFailures(int failures) {
    service_->consecutive_dhcp_failures_ = failures;
  }

  int GetLastDHCPOptionFailure() {
    return service_->last_dhcp_option_failure_.monotonic.tv_sec;
  }

  void SetLastDHCPOptionFailure(int monotonic_seconds) {
    service_->last_dhcp_option_failure_ = GetTimestamp(monotonic_seconds, "");
  }

  Service::DHCPOptionFailureState GetDHCPOptionFailureState() {
    return service_->dhcp_option_failure_state_;
  }

  void SetDHCPOptionFailureState(
      Service::DHCPOptionFailureState failure_state) {
    service_->dhcp_option_failure_state_ = failure_state;
  }

  int GetMaxDHCPOptionFailures() {
    return Service::kMaxDHCPOptionFailures;
  }

  int GetDHCPOptionHoldOffPeriodSeconds() {
    return Service::kDHCPOptionHoldOffPeriodSeconds;
  }

  MockManager mock_manager_;
  MockDiagnosticsReporter diagnostics_reporter_;
  MockTime time_;
  scoped_refptr<ServiceUnderTest> service_;
  scoped_refptr<ServiceUnderTest> service2_;
  string storage_id_;
  NiceMock<MockProxyFactory> proxy_factory_;
  MockPowerManager *power_manager_;  // Owned by |mock_manager_|.
  MockEapCredentials *eap_;  // Owned by |service_|.
};

class AllMockServiceTest : public testing::Test {
 public:
  AllMockServiceTest()
      : metrics_(&dispatcher_),
        manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        service_(new ServiceUnderTest(&control_interface_,
                                      &dispatcher_,
                                      &metrics_,
                                      &manager_)) { }
  virtual ~AllMockServiceTest() {}

 protected:
  MockControl control_interface_;
  StrictMock<MockEventDispatcher> dispatcher_;
  MockGLib glib_;
  NiceMock<MockMetrics> metrics_;
  MockManager manager_;
  scoped_refptr<ServiceUnderTest> service_;
};

TEST_F(ServiceTest, Constructor) {
  EXPECT_TRUE(service_->save_credentials_);
  EXPECT_EQ(Service::kCheckPortalAuto, service_->check_portal_);
  EXPECT_EQ(Service::kStateIdle, service_->state());
  EXPECT_FALSE(service_->has_ever_connected());
  EXPECT_EQ(0, service_->previous_error_serial_number_);
  EXPECT_EQ("", service_->previous_error_);
}

TEST_F(ServiceTest, CalculateState) {
  service_->state_ = Service::kStateConnected;
  Error error;
  EXPECT_EQ(kStateReady, service_->CalculateState(&error));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(ServiceTest, CalculateTechnology) {
  service_->technology_ = Technology::kWifi;
  Error error;
  EXPECT_EQ(kTypeWifi, service_->CalculateTechnology(&error));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(ServiceTest, GetProperties) {
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    string expected("true");
    service_->mutable_store()->SetStringProperty(kCheckPortalProperty,
                                                 expected,
                                                 &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(kCheckPortalProperty) == props.end());
    EXPECT_EQ(props[kCheckPortalProperty].reader().get_string(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    bool expected = true;
    service_->mutable_store()->SetBoolProperty(kAutoConnectProperty,
                                               expected,
                                               &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(kAutoConnectProperty) == props.end());
    EXPECT_EQ(props[kAutoConnectProperty].reader().get_bool(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(kConnectableProperty) == props.end());
    EXPECT_EQ(props[kConnectableProperty].reader().get_bool(), false);
  }
  {
    ::DBus::Error dbus_error;
    int32 expected = 127;
    service_->mutable_store()->SetInt32Property(kPriorityProperty,
                                                expected,
                                                &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(kPriorityProperty) == props.end());
    EXPECT_EQ(props[kPriorityProperty].reader().get_int32(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(kDeviceProperty) == props.end());
    EXPECT_EQ(props[kDeviceProperty].reader().get_path(),
              string(ServiceUnderTest::kRpcId));
  }
}

TEST_F(ServiceTest, SetProperty) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         kSaveCredentialsProperty,
                                         PropertyStoreTest::kBoolV,
                                         &error));
  }
  {
    ::DBus::Error error;
    ::DBus::Variant priority;
    priority.writer().append_int32(1);
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         kPriorityProperty,
                                         priority,
                                         &error));
  }
  {
    ::DBus::Error error;
    ::DBus::Variant guid;
    guid.writer().append_string("not default");
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         kGuidProperty,
                                         guid,
                                         &error));
  }
  // Ensure that EAP properties cannot be set on services with no EAP
  // credentials.  Use service2_ here since we're have some code in
  // ServiceTest::SetUp() that fiddles with service_->eap_.
  {
    ::DBus::Error error;
    ::DBus::Variant eap;
    eap.writer().append_string("eap eep eip!");
    EXPECT_FALSE(DBusAdaptor::SetProperty(service2_->mutable_store(),
                                          kEAPEAPProperty,
                                          eap,
                                          &error));
    ASSERT_TRUE(error.is_set());  // name() may be invalid otherwise
    EXPECT_EQ(invalid_prop(), error.name());
    // Now plumb in eap credentials, and try again.
    service2_->SetEapCredentials(new EapCredentials());
    EXPECT_TRUE(DBusAdaptor::SetProperty(service2_->mutable_store(),
                                         kEAPEAPProperty,
                                         eap,
                                         &error));
  }
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                          kConnectableProperty,
                                          PropertyStoreTest::kBoolV,
                                          &error));
    ASSERT_TRUE(error.is_set());  // name() may be invalid otherwise
    EXPECT_EQ(invalid_args(), error.name());
  }
  {
    ::DBus::Error error;
    ::DBus::Variant auto_connect;
    auto_connect.writer().append_bool(true);
    EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                         kAutoConnectProperty,
                                         auto_connect,
                                         &error));
  }
  // Ensure that we can perform a trivial set of the Name property (to its
  // current value) but an attempt to set the property to a different value
  // fails.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                          kNameProperty,
                                          DBusAdaptor::StringToVariant(
                                              GetFriendlyName()),
                                          &error));
    EXPECT_FALSE(error.is_set());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                          kNameProperty,
                                          PropertyStoreTest::kStringV,
                                          &error));
    ASSERT_TRUE(error.is_set());  // name() may be invalid otherwise
    EXPECT_EQ(invalid_args(), error.name());
  }
}

TEST_F(ServiceTest, GetLoadableStorageIdentifier) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_EQ("", service_->GetLoadableStorageIdentifier(storage));
  EXPECT_EQ(storage_id_, service_->GetLoadableStorageIdentifier(storage));
}

TEST_F(ServiceTest, IsLoadableFrom) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_FALSE(service_->IsLoadableFrom(storage));
  EXPECT_TRUE(service_->IsLoadableFrom(storage));
}

TEST_F(ServiceTest, Load) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(true));
  const string kCheckPortal("check-portal");
  const string kGUID("guid");
  const int kPriority = 20;
  const string kProxyConfig("proxy-config");
  const string kUIData("ui-data");
  EXPECT_CALL(storage, GetString(storage_id_, _, _)).Times(AnyNumber());
  EXPECT_CALL(storage, GetInt(storage_id_, _, _)).Times(AnyNumber());
  EXPECT_CALL(storage, GetString(storage_id_, Service::kStorageCheckPortal, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kCheckPortal), Return(true)));
  EXPECT_CALL(storage, GetString(storage_id_, Service::kStorageGUID, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kGUID), Return(true)));
  EXPECT_CALL(storage, GetInt(storage_id_, Service::kStoragePriority, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kPriority), Return(true)));
  EXPECT_CALL(storage, GetString(storage_id_, Service::kStorageProxyConfig, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kProxyConfig), Return(true)));
  EXPECT_CALL(storage, GetString(storage_id_, Service::kStorageUIData, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kUIData), Return(true)));
  EXPECT_CALL(storage, GetBool(storage_id_, _, _)).Times(AnyNumber());
  EXPECT_CALL(storage,
              GetBool(storage_id_, Service::kStorageSaveCredentials, _));
  EXPECT_CALL(*eap_, Load(&storage, storage_id_));
  EXPECT_TRUE(service_->Load(&storage));

  EXPECT_EQ(kCheckPortal, service_->check_portal_);
  EXPECT_EQ(kGUID, service_->guid_);
  EXPECT_EQ(kProxyConfig, service_->proxy_config_);
  EXPECT_EQ(kUIData, service_->ui_data_);

  Mock::VerifyAndClearExpectations(&storage);
  Mock::VerifyAndClearExpectations(eap_);

  // Assure that parameters are set to default if not available in the profile.
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(true));
  EXPECT_CALL(storage, GetBool(storage_id_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(storage, GetString(storage_id_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(storage, GetInt(storage_id_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*eap_, Load(&storage, storage_id_));
  EXPECT_TRUE(service_->Load(&storage));

  EXPECT_EQ(Service::kCheckPortalAuto, service_->check_portal_);
  EXPECT_EQ("", service_->guid_);
  EXPECT_EQ("", service_->proxy_config_);
  EXPECT_EQ("", service_->ui_data_);
}

TEST_F(ServiceTest, LoadFail) {
  StrictMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(false));
  EXPECT_FALSE(service_->Load(&storage));
}

TEST_F(ServiceTest, LoadAutoConnect) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(storage, GetBool(storage_id_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(storage, GetString(storage_id_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(storage, GetInt(storage_id_, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*eap_, Load(&storage, storage_id_)).Times(AnyNumber());

  // Three of each expectation so we can test Favorite == unset, false, true.
  EXPECT_CALL(storage, GetBool(storage_id_, Service::kStorageAutoConnect, _))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  EXPECT_CALL(storage, GetBool(storage_id_, Service::kStorageFavorite, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));

  // AutoConnect is unset, Favorite is unset.
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_FALSE(service_->retain_auto_connect());

  // AutoConnect is unset, Favorite is false.
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_FALSE(service_->retain_auto_connect());

  // AutoConnect is unset, Favorite is true.
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_TRUE(service_->retain_auto_connect());

  // AutoConnect is false, Favorite is unset.
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_TRUE(service_->retain_auto_connect());

  // AutoConnect is false, Favorite is false.
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_FALSE(service_->retain_auto_connect());

  // AutoConnect is false, Favorite is true.
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_TRUE(service_->retain_auto_connect());

  // AutoConnect is true, Favorite is unset.
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_TRUE(service_->auto_connect());
  EXPECT_TRUE(service_->retain_auto_connect());

  // AutoConnect is true, Favorite is false (invalid case).
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_TRUE(service_->auto_connect());
  EXPECT_FALSE(service_->retain_auto_connect());

  // AutoConnect is true, Favorite is true.
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_TRUE(service_->auto_connect());
  EXPECT_TRUE(service_->retain_auto_connect());
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
  EXPECT_CALL(storage, DeleteKey(storage_id_, Service::kStorageFavorite))
      .WillOnce(Return(true));
  EXPECT_CALL(storage, DeleteKey(storage_id_, Service::kStorageAutoConnect))
      .WillOnce(Return(true));
  EXPECT_CALL(storage, SetBool(storage_id_, _, _)).Times(AnyNumber());
  EXPECT_CALL(storage,
              SetBool(storage_id_,
                      Service::kStorageSaveCredentials,
                      service_->save_credentials()));
  EXPECT_CALL(*eap_, Save(&storage, storage_id_, true));
  EXPECT_TRUE(service_->Save(&storage));
}

TEST_F(ServiceTest, RetainAutoConnect) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, SetString(storage_id_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(storage, DeleteKey(storage_id_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(storage, DeleteKey(storage_id_, Service::kStorageFavorite))
      .Times(2);
  EXPECT_CALL(storage, DeleteKey(storage_id_, Service::kStorageAutoConnect))
      .Times(0);
  EXPECT_CALL(storage, SetBool(storage_id_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*eap_, Save(&storage, storage_id_, true)).Times(2);

  // AutoConnect flag set true.
  service_->EnableAndRetainAutoConnect();
  EXPECT_CALL(storage,
              SetBool(storage_id_, Service::kStorageAutoConnect, true));
  EXPECT_TRUE(service_->Save(&storage));

  // AutoConnect flag set false.
  EXPECT_CALL(storage,
              SetBool(storage_id_, Service::kStorageAutoConnect, false));
  service_->SetAutoConnect(false);
  EXPECT_TRUE(service_->Save(&storage));
}

TEST_F(ServiceTest, Unload) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(true));
  static const string string_value("value");
  EXPECT_CALL(storage, GetString(storage_id_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(string_value), Return(true)));
  EXPECT_CALL(storage, GetBool(storage_id_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(true), Return(true)));
  EXPECT_FALSE(service_->explicitly_disconnected_);
  service_->explicitly_disconnected_ = true;
  EXPECT_FALSE(service_->has_ever_connected_);
  EXPECT_CALL(*eap_, Load(&storage, storage_id_));
  ASSERT_TRUE(service_->Load(&storage));
  // TODO(pstew): Only two string properties in the service are tested as
  // a sentinel that properties are being set and reset at the right times.
  // However, since property load/store is essentially a manual process,
  // it is error prone and should either be exhaustively unit-tested or
  // a generic framework for registering loaded/stored properties should
  // be created. crbug.com/207798
  EXPECT_EQ(string_value, service_->ui_data_);
  EXPECT_EQ(string_value, service_->guid_);
  EXPECT_FALSE(service_->explicitly_disconnected_);
  EXPECT_TRUE(service_->has_ever_connected_);
  service_->explicitly_disconnected_ = true;
  SetConsecutiveDHCPFailures(100);
  SetLastDHCPOptionFailure(200);
  SetDHCPOptionFailureState(Service::kDHCPOptionFailureConfirmed);
  EXPECT_CALL(*eap_, Reset());
  service_->Unload();
  EXPECT_EQ(string(""), service_->ui_data_);
  EXPECT_EQ(string(""), service_->guid_);
  EXPECT_FALSE(service_->explicitly_disconnected_);
  EXPECT_FALSE(service_->has_ever_connected_);
  EXPECT_EQ(0, GetConsecutiveDHCPFailures());
  EXPECT_EQ(0, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureNotDetected,
            GetDHCPOptionFailureState());
}

TEST_F(ServiceTest, State) {
  EXPECT_EQ(Service::kStateIdle, service_->state());
  EXPECT_EQ(Service::kStateIdle, GetPreviousState());
  EXPECT_EQ(Service::kFailureUnknown, service_->failure());
  const string unknown_error(
      Service::ConnectFailureToString(Service::kFailureUnknown));
  EXPECT_EQ(unknown_error, service_->error());

  ServiceRefPtr service_ref(service_);

  EXPECT_CALL(*GetAdaptor(),
              EmitStringChanged(kStateProperty, _)).Times(6);
  EXPECT_CALL(*GetAdaptor(),
              EmitStringChanged(kErrorProperty, _)).Times(4);
  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetState(Service::kStateConnected);
  EXPECT_EQ(Service::kStateIdle, GetPreviousState());
  // A second state change shouldn't cause another update
  service_->SetState(Service::kStateConnected);
  EXPECT_EQ(Service::kStateConnected, service_->state());
  EXPECT_EQ(Service::kStateIdle, GetPreviousState());
  EXPECT_EQ(Service::kFailureUnknown, service_->failure());
  EXPECT_TRUE(service_->has_ever_connected_);

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetFailure(Service::kFailureOutOfRange);
  EXPECT_TRUE(service_->IsFailed());
  EXPECT_GT(service_->failed_time_, 0);
  EXPECT_GT(service_->previous_error_serial_number_, 0);
  EXPECT_EQ(Service::kStateFailure, service_->state());
  EXPECT_EQ(Service::kFailureOutOfRange, service_->failure());
  const string out_of_range_error(
      Service::ConnectFailureToString(Service::kFailureOutOfRange));
  EXPECT_EQ(out_of_range_error, service_->error());
  EXPECT_EQ(out_of_range_error, service_->previous_error_);

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetState(Service::kStateConnected);
  EXPECT_FALSE(service_->IsFailed());
  EXPECT_EQ(service_->failed_time_, 0);
  EXPECT_EQ(unknown_error, service_->error());
  EXPECT_EQ(out_of_range_error, service_->previous_error_);
  EXPECT_GT(service_->previous_error_serial_number_, 0);

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetFailureSilent(Service::kFailurePinMissing);
  EXPECT_TRUE(service_->IsFailed());
  EXPECT_GT(service_->failed_time_, 0);
  EXPECT_GT(service_->previous_error_serial_number_, 0);
  EXPECT_EQ(Service::kStateIdle, service_->state());
  EXPECT_EQ(Service::kFailurePinMissing, service_->failure());
  const string pin_missing_error(
      Service::ConnectFailureToString(Service::kFailurePinMissing));
  EXPECT_EQ(pin_missing_error, service_->error());
  EXPECT_EQ(pin_missing_error, service_->previous_error_);

  // If the Service has a Profile, the profile should be saved when
  // the service enters kStateConnected. (The case where the service
  // doesn't have a profile is tested above.)
  MockProfileRefPtr mock_profile(
      new MockProfile(control_interface(), metrics(), &mock_manager_));
  NiceMock<MockStore> storage;
  service_->set_profile(mock_profile);
  service_->has_ever_connected_ = false;
  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  EXPECT_CALL(*mock_profile, GetConstStorage())
      .WillOnce(Return(&storage));
  EXPECT_CALL(*mock_profile, UpdateService(service_ref));
  service_->SetState(Service::kStateConnected);
  EXPECT_TRUE(service_->has_ever_connected_);
  service_->set_profile(NULL);  // Break reference cycle.

  // Similar to the above, but emulate an emphemeral profile, which
  // has no storage. We can't update the service in the profile, but
  // we should not crash.
  service_->state_ = Service::kStateIdle;  // Skips state change logic.
  service_->set_profile(mock_profile);
  service_->has_ever_connected_ = false;
  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  EXPECT_CALL(*mock_profile, GetConstStorage()).
      WillOnce(Return(static_cast<StoreInterface *>(NULL)));
  service_->SetState(Service::kStateConnected);
  EXPECT_TRUE(service_->has_ever_connected_);
  service_->set_profile(NULL);  // Break reference cycle.
}

TEST_F(ServiceTest, StateResetAfterFailure) {
  service_->SetFailure(Service::kFailureOutOfRange);
  EXPECT_EQ(Service::kStateFailure, service_->state());
  Error error;
  service_->Connect(&error, "in test");
  EXPECT_EQ(Service::kStateIdle, service_->state());
  EXPECT_EQ(Service::kFailureUnknown, service_->failure());

  service_->SetState(Service::kStateConnected);
  service_->Connect(&error, "in test");
  EXPECT_EQ(Service::kStateConnected, service_->state());
}

TEST_F(ServiceTest, ActivateCellularModem) {
  ResultCallback callback =
      Bind(&ServiceTest::TestCallback, Unretained(this));
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  Error error;
  service_->ActivateCellularModem("Carrier", &error, callback);
  EXPECT_TRUE(error.IsFailure());
}

TEST_F(ServiceTest, CompleteCellularActivation) {
  Error error;
  service_->CompleteCellularActivation(&error);
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(ServiceTest, EnableAndRetainAutoConnect) {
  EXPECT_FALSE(service_->retain_auto_connect());
  EXPECT_FALSE(service_->auto_connect());

  service_->EnableAndRetainAutoConnect();
  EXPECT_TRUE(service_->retain_auto_connect());
  EXPECT_TRUE(service_->auto_connect());
}

TEST_F(ServiceTest, ReRetainAutoConnect) {
  service_->EnableAndRetainAutoConnect();
  EXPECT_TRUE(service_->retain_auto_connect());
  EXPECT_TRUE(service_->auto_connect());

  service_->SetAutoConnect(false);
  service_->EnableAndRetainAutoConnect();
  EXPECT_TRUE(service_->retain_auto_connect());
  EXPECT_FALSE(service_->auto_connect());
}

TEST_F(ServiceTest, IsAutoConnectable) {
  const char *reason = NULL;
  service_->SetConnectable(true);

  // Services with non-primary connectivity technologies should not auto-connect
  // when the system is offline.
  EXPECT_EQ(Technology::kUnknown, service_->technology());
  EXPECT_CALL(mock_manager_, IsOnline()).WillOnce(Return(false));
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnOffline, reason);

  service_->technology_ = Technology::kEthernet;
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // We should not auto-connect to a Service that a user has
  // deliberately disconnected.
  Error error;
  service_->UserInitiatedDisconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnExplicitDisconnect, reason);

  // But if the Service is reloaded, it is eligible for auto-connect
  // again.
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillOnce(Return(true));
  EXPECT_CALL(*eap_, Load(&storage, storage_id_));
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // A deliberate Connect should also re-enable auto-connect.
  service_->UserInitiatedDisconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  service_->Connect(&error, "in test");
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // A non-user initiated Disconnect doesn't change anything.
  service_->Disconnect(&error);
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // A resume also re-enables auto-connect.
  service_->UserInitiatedDisconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  service_->OnAfterResume();
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  service_->SetState(Service::kStateConnected);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnConnected, reason);

  service_->SetState(Service::kStateAssociating);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnConnecting, reason);
}

TEST_F(ServiceTest, AutoConnectLogging) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _));
  service_->SetConnectable(true);

  ScopeLogger::GetInstance()->EnableScopesByName("+service");
  ScopeLogger::GetInstance()->set_verbose_level(1);
  service_->SetState(Service::kStateConnected);
  EXPECT_CALL(log, Log(-1, _, HasSubstr(Service::kAutoConnConnected)));
  service_->AutoConnect();

  ScopeLogger::GetInstance()->EnableScopesByName("-service");
  ScopeLogger::GetInstance()->set_verbose_level(0);
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       HasSubstr(Service::kAutoConnNotConnectable)));
  service_->SetConnectable(false);
  service_->AutoConnect();
}


TEST_F(AllMockServiceTest, AutoConnectWithFailures) {
  const char *reason;
  service_->SetConnectable(true);
  service_->technology_ = Technology::kEthernet;
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // The very first AutoConnect() doesn't trigger any throttling.
  EXPECT_CALL(dispatcher_, PostDelayedTask(_, _)).Times(0);
  service_->AutoConnect();
  Mock::VerifyAndClearExpectations(&dispatcher_);
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // The second call does trigger some throttling.
  EXPECT_CALL(dispatcher_, PostDelayedTask(_,
      Service::kMinAutoConnectCooldownTimeMilliseconds));
  service_->AutoConnect();
  Mock::VerifyAndClearExpectations(&dispatcher_);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnThrottled, reason);

  // Calling AutoConnect() again before the cooldown terminates does not change
  // the timeout.
  EXPECT_CALL(dispatcher_, PostDelayedTask(_, _)).Times(0);
  service_->AutoConnect();
  Mock::VerifyAndClearExpectations(&dispatcher_);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnThrottled, reason);

  // Once the timeout expires, we can AutoConnect() again.
  service_->ReEnableAutoConnectTask();
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // Timeouts increase exponentially.
  uint64 next_cooldown_time = service_->auto_connect_cooldown_milliseconds_;
  EXPECT_EQ(next_cooldown_time,
            Service::kAutoConnectCooldownBackoffFactor *
            Service::kMinAutoConnectCooldownTimeMilliseconds);
  while (next_cooldown_time <=
         Service::kMaxAutoConnectCooldownTimeMilliseconds) {
    EXPECT_CALL(dispatcher_, PostDelayedTask(_, next_cooldown_time));
    service_->AutoConnect();
    Mock::VerifyAndClearExpectations(&dispatcher_);
    EXPECT_FALSE(service_->IsAutoConnectable(&reason));
    EXPECT_STREQ(Service::kAutoConnThrottled, reason);
    service_->ReEnableAutoConnectTask();
    next_cooldown_time *= Service::kAutoConnectCooldownBackoffFactor;
  }

  // Once we hit our cap, future timeouts are the same.
  for (int32 i = 0; i < 2; i++) {
    EXPECT_CALL(dispatcher_, PostDelayedTask(_,
        Service::kMaxAutoConnectCooldownTimeMilliseconds));
    service_->AutoConnect();
    Mock::VerifyAndClearExpectations(&dispatcher_);
    EXPECT_FALSE(service_->IsAutoConnectable(&reason));
    EXPECT_STREQ(Service::kAutoConnThrottled, reason);
    service_->ReEnableAutoConnectTask();
  }

  // Connecting successfully resets our cooldown.
  service_->SetState(Service::kStateConnected);
  service_->SetState(Service::kStateIdle);
  reason = "";
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ("", reason);
  EXPECT_EQ(service_->auto_connect_cooldown_milliseconds_, 0);

  // But future AutoConnects behave as before
  EXPECT_CALL(dispatcher_, PostDelayedTask(_,
      Service::kMinAutoConnectCooldownTimeMilliseconds)).Times(1);
  service_->AutoConnect();
  service_->AutoConnect();
  Mock::VerifyAndClearExpectations(&dispatcher_);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnThrottled, reason);

  // Cooldowns are forgotten if we go through a suspend/resume cycle.
  service_->OnAfterResume();
  reason = "";
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ("", reason);
}

TEST_F(ServiceTest, ConfigureBadProperty) {
  KeyValueStore args;
  args.SetString("XXXInvalid", "Value");
  Error error;
  service_->Configure(args, &error);
  EXPECT_FALSE(error.IsSuccess());
}

TEST_F(ServiceTest, ConfigureBoolProperty) {
  service_->EnableAndRetainAutoConnect();
  service_->SetAutoConnect(false);
  ASSERT_FALSE(service_->auto_connect());
  KeyValueStore args;
  args.SetBool(kAutoConnectProperty, true);
  Error error;
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(service_->auto_connect());
}

TEST_F(ServiceTest, ConfigureStringProperty) {
  const string kGuid0 = "guid_zero";
  const string kGuid1 = "guid_one";
  service_->SetGuid(kGuid0, NULL);
  ASSERT_EQ(kGuid0, service_->guid());
  KeyValueStore args;
  args.SetString(kGuidProperty, kGuid1);
  Error error;
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kGuid1, service_->guid());
}

TEST_F(ServiceTest, ConfigureStringsProperty) {
  const vector<string> kStrings0{ "string0", "string1" };
  const vector<string> kStrings1{ "string2", "string3" };
  service_->set_strings(kStrings0);
  ASSERT_EQ(kStrings0, service_->strings());
  KeyValueStore args;
  args.SetStrings(ServiceUnderTest::kStringsProperty, kStrings1);
  Error error;
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kStrings1, service_->strings());
}

TEST_F(ServiceTest, ConfigureEapStringProperty) {
  MockEapCredentials *eap = new MockEapCredentials();
  service2_->SetEapCredentials(eap);  // Passes ownership.

  const string kEAPManagement0 = "management_zero";
  const string kEAPManagement1 = "management_one";
  service2_->SetEAPKeyManagement(kEAPManagement0);

  EXPECT_CALL(*eap, key_management())
      .WillOnce(ReturnRef(kEAPManagement0));
  ASSERT_EQ(kEAPManagement0, service2_->GetEAPKeyManagement());
  KeyValueStore args;
  EXPECT_CALL(*eap, SetKeyManagement(kEAPManagement1, _));
  args.SetString(kEapKeyMgmtProperty, kEAPManagement1);
  Error error;
  service2_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(ServiceTest, ConfigureIntProperty) {
  const int kPriority0 = 100;
  const int kPriority1 = 200;
  service_->SetPriority(kPriority0, NULL);
  ASSERT_EQ(kPriority0, service_->priority());
  KeyValueStore args;
  args.SetInt(kPriorityProperty, kPriority1);
  Error error;
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kPriority1, service_->priority());
}

TEST_F(ServiceTest, ConfigureIgnoredProperty) {
  service_->EnableAndRetainAutoConnect();
  service_->SetAutoConnect(false);
  ASSERT_FALSE(service_->auto_connect());
  KeyValueStore args;
  args.SetBool(kAutoConnectProperty, true);
  Error error;
  service_->IgnoreParameterForConfigure(kAutoConnectProperty);
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_FALSE(service_->auto_connect());
}

TEST_F(ServiceTest, ConfigureProfileProperty) {
  // Ensure that the Profile property is always ignored.
  KeyValueStore args;
  args.SetString(kProfileProperty, "profile");
  Error error;
  EXPECT_CALL(mock_manager_, SetProfileForService(_, _, _)).Times(0);
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(ServiceTest, DoPropertiesMatch) {
  service_->SetAutoConnect(false);
  const string kGUID0 = "guid_zero";
  const string kGUID1 = "guid_one";
  service_->SetGuid(kGUID0, NULL);
  const uint32 kPriority0 = 100;
  const uint32 kPriority1 = 200;
  service_->SetPriority(kPriority0, NULL);
  const vector<string> kStrings0{ "string0", "string1" };
  const vector<string> kStrings1{ "string2", "string3" };
  service_->set_strings(kStrings0);

  {
    KeyValueStore args;
    args.SetString(kGuidProperty, kGUID0);
    args.SetBool(kAutoConnectProperty, false);
    args.SetInt(kPriorityProperty, kPriority0);
    args.SetStrings(ServiceUnderTest::kStringsProperty, kStrings0);
    EXPECT_TRUE(service_->DoPropertiesMatch(args));
  }
  {
    KeyValueStore args;
    args.SetString(kGuidProperty, kGUID1);
    args.SetBool(kAutoConnectProperty, false);
    args.SetInt(kPriorityProperty, kPriority0);
    args.SetStrings(ServiceUnderTest::kStringsProperty, kStrings0);
    EXPECT_FALSE(service_->DoPropertiesMatch(args));
  }
  {
    KeyValueStore args;
    args.SetString(kGuidProperty, kGUID0);
    args.SetBool(kAutoConnectProperty, true);
    args.SetInt(kPriorityProperty, kPriority0);
    args.SetStrings(ServiceUnderTest::kStringsProperty, kStrings0);
    EXPECT_FALSE(service_->DoPropertiesMatch(args));
  }
  {
    KeyValueStore args;
    args.SetString(kGuidProperty, kGUID0);
    args.SetBool(kAutoConnectProperty, false);
    args.SetInt(kPriorityProperty, kPriority1);
    args.SetStrings(ServiceUnderTest::kStringsProperty, kStrings0);
    EXPECT_FALSE(service_->DoPropertiesMatch(args));
  }
  {
    KeyValueStore args;
    args.SetString(kGuidProperty, kGUID0);
    args.SetBool(kAutoConnectProperty, false);
    args.SetInt(kPriorityProperty, kPriority0);
    args.SetStrings(ServiceUnderTest::kStringsProperty, kStrings1);
    EXPECT_FALSE(service_->DoPropertiesMatch(args));
  }
}

TEST_F(ServiceTest, IsRemembered) {
  ServiceConstRefPtr service_ref(service_);
  service_->set_profile(NULL);
  EXPECT_CALL(mock_manager_, IsServiceEphemeral(_)).Times(0);
  EXPECT_FALSE(service_->IsRemembered());

  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), metrics(), manager()));
  service_->set_profile(profile);
  EXPECT_CALL(mock_manager_, IsServiceEphemeral(service_ref))
     .WillOnce(Return(true))
     .WillOnce(Return(false));
  EXPECT_FALSE(service_->IsRemembered());
  EXPECT_TRUE(service_->IsRemembered());
}

TEST_F(ServiceTest, IsDependentOn) {
  EXPECT_FALSE(service_->IsDependentOn(NULL));

  scoped_ptr<MockDeviceInfo> mock_device_info(
      new NiceMock<MockDeviceInfo>(control_interface(), dispatcher(), metrics(),
                                   &mock_manager_));
  scoped_refptr<MockConnection> mock_connection0(
      new NiceMock<MockConnection>(mock_device_info.get()));
  scoped_refptr<MockConnection> mock_connection1(
      new NiceMock<MockConnection>(mock_device_info.get()));

  service_->connection_ = mock_connection0;
  EXPECT_CALL(*mock_connection0, GetLowerConnection())
      .WillRepeatedly(Return(mock_connection1));
  EXPECT_CALL(*mock_connection1, GetLowerConnection())
      .WillRepeatedly(Return(ConnectionRefPtr()));
  EXPECT_FALSE(service_->IsDependentOn(NULL));

  scoped_refptr<ServiceUnderTest> service1 =
      new ServiceUnderTest(control_interface(),
                           dispatcher(),
                           metrics(),
                           &mock_manager_);
  EXPECT_FALSE(service_->IsDependentOn(service1));

  service1->connection_ = mock_connection0;
  EXPECT_FALSE(service_->IsDependentOn(service1));

  service1->connection_ = mock_connection1;
  EXPECT_TRUE(service_->IsDependentOn(service1));

  service_->connection_ = mock_connection1;
  service1->connection_ = NULL;
  EXPECT_FALSE(service_->IsDependentOn(service1));

  service_->connection_ = NULL;
}

TEST_F(ServiceTest, OnPropertyChanged) {
  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), metrics(), manager()));
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
  service_->OnPropertyChanged(kCheckPortalProperty);

  service_->state_ = Service::kStatePortal;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(service_ref)).Times(1);
  service_->OnPropertyChanged(kCheckPortalProperty);

  service_->state_ = Service::kStateConnected;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(service_ref)).Times(1);
  service_->OnPropertyChanged(kProxyConfigProperty);

  service_->state_ = Service::kStateOnline;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(service_ref)).Times(1);
  service_->OnPropertyChanged(kCheckPortalProperty);

  service_->state_ = Service::kStatePortal;
  EXPECT_CALL(mock_manager_, RecheckPortalOnService(_)).Times(0);
  service_->OnPropertyChanged(kEAPKeyIDProperty);
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

TEST_F(ServiceTest, SetFriendlyName) {
  EXPECT_EQ(service_->unique_name_, service_->friendly_name_);
  ServiceMockAdaptor *adaptor = GetAdaptor();

  EXPECT_CALL(*adaptor, EmitStringChanged(_, _)).Times(0);
  service_->SetFriendlyName(service_->unique_name_);
  EXPECT_EQ(service_->unique_name_, service_->friendly_name_);

  EXPECT_CALL(*adaptor, EmitStringChanged(kNameProperty,
                                          "Test Name 1"));
  service_->SetFriendlyName("Test Name 1");
  EXPECT_EQ("Test Name 1", service_->friendly_name_);

  EXPECT_CALL(*adaptor, EmitStringChanged(_, _)).Times(0);
  service_->SetFriendlyName("Test Name 1");
  EXPECT_EQ("Test Name 1", service_->friendly_name_);

  EXPECT_CALL(*adaptor, EmitStringChanged(kNameProperty,
                                          "Test Name 2"));
  service_->SetFriendlyName("Test Name 2");
  EXPECT_EQ("Test Name 2", service_->friendly_name_);
}

TEST_F(ServiceTest, SetConnectableFull) {
  EXPECT_FALSE(service_->connectable());

  ServiceMockAdaptor *adaptor = GetAdaptor();

  EXPECT_CALL(*adaptor, EmitBoolChanged(_, _)).Times(0);
  EXPECT_CALL(mock_manager_, HasService(_)).Times(0);
  service_->SetConnectableFull(false);
  EXPECT_FALSE(service_->connectable());

  EXPECT_CALL(*adaptor, EmitBoolChanged(kConnectableProperty, true));
  EXPECT_CALL(mock_manager_, HasService(_)).WillOnce(Return(false));
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(0);
  service_->SetConnectableFull(true);
  EXPECT_TRUE(service_->connectable());

  EXPECT_CALL(*adaptor, EmitBoolChanged(kConnectableProperty, false));
  EXPECT_CALL(mock_manager_, HasService(_)).WillOnce(Return(true));
  EXPECT_CALL(mock_manager_, UpdateService(_));
  service_->SetConnectableFull(false);
  EXPECT_FALSE(service_->connectable());

  EXPECT_CALL(*adaptor, EmitBoolChanged(kConnectableProperty, true));
  EXPECT_CALL(mock_manager_, HasService(_)).WillOnce(Return(true));
              EXPECT_CALL(mock_manager_, UpdateService(_));
  service_->SetConnectableFull(true);
  EXPECT_TRUE(service_->connectable());
}

class WriteOnlyServicePropertyTest : public ServiceTest {};
TEST_P(WriteOnlyServicePropertyTest, PropertyWriteOnly) {
  // Use a real EapCredentials instance since the base Service class
  // contains no write-only properties.
  EapCredentials eap;
  eap.InitPropertyStore(service_->mutable_store());

  string property(GetParam().reader().get_string());
  Error error;
  EXPECT_FALSE(service_->store().GetStringProperty(property, NULL, &error));
  EXPECT_EQ(Error::kPermissionDenied, error.type());
}

INSTANTIATE_TEST_CASE_P(
    WriteOnlyServicePropertyTestInstance,
    WriteOnlyServicePropertyTest,
    Values(
        DBusAdaptor::StringToVariant(kEapPrivateKeyPasswordProperty),
        DBusAdaptor::StringToVariant(kEapPasswordProperty)));


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

class ServiceWithMockOnEapCredentialsChanged : public ServiceUnderTest {
 public:
  ServiceWithMockOnEapCredentialsChanged(ControlInterface *control_interface,
                                         EventDispatcher *dispatcher,
                                         Metrics *metrics,
                                         Manager *manager)
      : ServiceUnderTest(control_interface, dispatcher, metrics, manager),
        is_8021x_(false) {}
  MOCK_METHOD0(OnEapCredentialsChanged, void());
  virtual bool Is8021x() const { return is_8021x_; }
  void set_is_8021x(bool is_8021x) { is_8021x_ = is_8021x; }

 private:
  bool is_8021x_;
};

TEST_F(ServiceTest, SetEAPCredentialsOverRPC) {
  scoped_refptr<ServiceWithMockOnEapCredentialsChanged> service(
      new ServiceWithMockOnEapCredentialsChanged(control_interface(),
                                                 dispatcher(),
                                                 metrics(),
                                                 &mock_manager_));
  string eap_credential_properties[] = {
      kEapAnonymousIdentityProperty,
      kEAPCertIDProperty,
      kEAPClientCertProperty,
      kEapIdentityProperty,
      kEAPKeyIDProperty,
      kEapPasswordProperty,
      kEAPPINProperty,
      kEapPrivateKeyProperty,
      kEapPrivateKeyPasswordProperty
  };
  string eap_non_credential_properties[] = {
      kEapCaCertIDProperty,
      kEapCaCertNssProperty,
      kEAPEAPProperty,
      kEapPhase2AuthProperty,
      kEapUseSystemCAsProperty
  };
  // While this is not an 802.1x-based service, none of these property
  // changes should cause a call to set_eap().
  EXPECT_CALL(*service, OnEapCredentialsChanged()).Times(0);
  for (size_t i = 0; i < arraysize(eap_credential_properties); ++i)
    service->OnPropertyChanged(eap_credential_properties[i]);
  for (size_t i = 0; i < arraysize(eap_non_credential_properties); ++i)
    service->OnPropertyChanged(eap_non_credential_properties[i]);
  service->OnPropertyChanged(kEapKeyMgmtProperty);

  service->set_is_8021x(true);

  // When this is an 802.1x-based service, set_eap should be called for
  // all credential-carrying properties.
  for (size_t i = 0; i < arraysize(eap_credential_properties); ++i) {
    EXPECT_CALL(*service, OnEapCredentialsChanged()).Times(1);
    service->OnPropertyChanged(eap_credential_properties[i]);
    Mock::VerifyAndClearExpectations(service.get());
  }

  // The key management property is a special case.  While not strictly
  // a credential, it can change which credentials are used.  Therefore it
  // should also trigger a call to set_eap();
  EXPECT_CALL(*service, OnEapCredentialsChanged()).Times(1);
  service->OnPropertyChanged(kEapKeyMgmtProperty);
  Mock::VerifyAndClearExpectations(service.get());

  EXPECT_CALL(*service, OnEapCredentialsChanged()).Times(0);
  for (size_t i = 0; i < arraysize(eap_non_credential_properties); ++i)
    service->OnPropertyChanged(eap_non_credential_properties[i]);
}

TEST_F(ServiceTest, Certification) {
  EXPECT_FALSE(service_->remote_certification_.size());

  ScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_WARNING, _,
                       HasSubstr("exceeds our maximum"))).Times(2);
  string kSubject("foo");
  EXPECT_FALSE(service_->AddEAPCertification(
      kSubject, Service::kEAPMaxCertificationElements));
  EXPECT_FALSE(service_->AddEAPCertification(
      kSubject, Service::kEAPMaxCertificationElements + 1));
  EXPECT_FALSE(service_->remote_certification_.size());
  Mock::VerifyAndClearExpectations(&log);

  EXPECT_CALL(log,
              Log(logging::LOG_INFO, _, HasSubstr("Received certification")))
      .Times(1);
  EXPECT_TRUE(service_->AddEAPCertification(
      kSubject, Service::kEAPMaxCertificationElements - 1));
  Mock::VerifyAndClearExpectations(&log);
  EXPECT_EQ(Service::kEAPMaxCertificationElements,
      service_->remote_certification_.size());
  for (size_t i = 0; i < Service::kEAPMaxCertificationElements - 1; ++i) {
      EXPECT_TRUE(service_->remote_certification_[i].empty());
  }
  EXPECT_EQ(kSubject, service_->remote_certification_[
      Service::kEAPMaxCertificationElements - 1]);

  // Re-adding the same name in the same position should not generate a log.
  EXPECT_CALL(log, Log(_, _, _)).Times(0);
  EXPECT_TRUE(service_->AddEAPCertification(
      kSubject, Service::kEAPMaxCertificationElements - 1));

  // Replacing the item should generate a log message.
  EXPECT_CALL(log,
              Log(logging::LOG_INFO, _, HasSubstr("Received certification")))
      .Times(1);
  EXPECT_TRUE(service_->AddEAPCertification(
      kSubject + "x", Service::kEAPMaxCertificationElements - 1));

  service_->ClearEAPCertification();
  EXPECT_TRUE(service_->remote_certification_.empty());
}

TEST_F(ServiceTest, NoteDisconnectEventIdle) {
  Timestamp timestamp;
  EXPECT_CALL(time_, GetNow()).Times(4).WillRepeatedly((Return(timestamp)));
  SetStateField(Service::kStateOnline);
  EXPECT_FALSE(service_->HasRecentConnectionIssues());
  service_->SetState(Service::kStateIdle);
  // The transition Online->Idle is not an event.
  EXPECT_FALSE(service_->HasRecentConnectionIssues());
  service_->SetState(Service::kStateFailure);
  // The transition Online->Idle->Failure is a connection drop.
  EXPECT_TRUE(service_->HasRecentConnectionIssues());
}

TEST_F(ServiceTest, NoteDisconnectEventOnSetStateFailure) {
  Timestamp timestamp;
  EXPECT_CALL(time_, GetNow()).Times(3).WillRepeatedly((Return(timestamp)));
  SetStateField(Service::kStateOnline);
  EXPECT_FALSE(service_->HasRecentConnectionIssues());
  service_->SetState(Service::kStateFailure);
  EXPECT_TRUE(service_->HasRecentConnectionIssues());
}

TEST_F(ServiceTest, NoteDisconnectEventOnSetFailureSilent) {
  Timestamp timestamp;
  EXPECT_CALL(time_, GetNow()).Times(3).WillRepeatedly((Return(timestamp)));
  SetStateField(Service::kStateConfiguring);
  EXPECT_FALSE(service_->HasRecentConnectionIssues());
  service_->SetFailureSilent(Service::kFailureEAPAuthentication);
  EXPECT_TRUE(service_->HasRecentConnectionIssues());
}

TEST_F(ServiceTest, NoteDisconnectEventNonEvent) {
  EXPECT_CALL(time_, GetNow()).Times(0);
  EXPECT_CALL(diagnostics_reporter_, OnConnectivityEvent()).Times(0);

  // Explicit disconnect is a non-event.
  SetStateField(Service::kStateOnline);
  SetExplicitlyDisconnected(true);
  NoteDisconnectEvent();
  EXPECT_TRUE(GetDisconnects()->empty());
  EXPECT_TRUE(GetMisconnects()->empty());

  // Failure to idle transition is a non-event.
  SetStateField(Service::kStateFailure);
  SetExplicitlyDisconnected(false);
  NoteDisconnectEvent();
  EXPECT_TRUE(GetDisconnects()->empty());
  EXPECT_TRUE(GetMisconnects()->empty());

  // Disconnect while manager is stopped is a non-event.
  SetStateField(Service::kStateOnline);
  SetManagerRunning(false);
  NoteDisconnectEvent();
  EXPECT_TRUE(GetDisconnects()->empty());
  EXPECT_TRUE(GetMisconnects()->empty());

  // Disconnect while suspending is a non-event.
  SetManagerRunning(true);
  SetSuspending(true);
  NoteDisconnectEvent();
  EXPECT_TRUE(GetDisconnects()->empty());
  EXPECT_TRUE(GetMisconnects()->empty());
}

TEST_F(ServiceTest, NoteDisconnectEventDisconnectOnce) {
  const int kNow = 5;
  EXPECT_FALSE(service_->explicitly_disconnected());
  SetStateField(Service::kStateOnline);
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(kNow, "")));
  EXPECT_CALL(diagnostics_reporter_, OnConnectivityEvent()).Times(0);
  NoteDisconnectEvent();
  ASSERT_EQ(1, GetDisconnects()->size());
  EXPECT_EQ(kNow, GetDisconnects()->front().monotonic.tv_sec);
  EXPECT_TRUE(GetMisconnects()->empty());

  Mock::VerifyAndClearExpectations(&time_);
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(
      kNow  + GetDisconnectsMonitorSeconds() - 1, "")));
  EXPECT_TRUE(service_->HasRecentConnectionIssues());
  ASSERT_EQ(1, GetDisconnects()->size());

  Mock::VerifyAndClearExpectations(&time_);
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(
      kNow + GetDisconnectsMonitorSeconds(), "")));
  EXPECT_FALSE(service_->HasRecentConnectionIssues());
  ASSERT_TRUE(GetDisconnects()->empty());
}

TEST_F(ServiceTest, NoteDisconnectEventDisconnectThreshold) {
  EXPECT_FALSE(service_->explicitly_disconnected());
  SetStateField(Service::kStateOnline);
  const int kNow = 6;
  for (int i = 0; i < GetReportDisconnectsThreshold() - 1; i++) {
    PushTimestamp(GetDisconnects(), kNow, "");
  }
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(kNow, "")));
  EXPECT_CALL(diagnostics_reporter_, OnConnectivityEvent()).Times(1);
  NoteDisconnectEvent();
  EXPECT_EQ(GetReportDisconnectsThreshold(), GetDisconnects()->size());
}

TEST_F(ServiceTest, NoteDisconnectEventMisconnectOnce) {
  const int kNow = 7;
  EXPECT_FALSE(service_->explicitly_disconnected());
  SetStateField(Service::kStateConfiguring);
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(kNow, "")));
  EXPECT_CALL(diagnostics_reporter_, OnConnectivityEvent()).Times(0);
  NoteDisconnectEvent();
  EXPECT_TRUE(GetDisconnects()->empty());
  ASSERT_EQ(1, GetMisconnects()->size());
  EXPECT_EQ(kNow, GetMisconnects()->front().monotonic.tv_sec);

  Mock::VerifyAndClearExpectations(&time_);
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(
      kNow + GetMisconnectsMonitorSeconds() - 1, "")));
  EXPECT_TRUE(service_->HasRecentConnectionIssues());
  ASSERT_EQ(1, GetMisconnects()->size());

  Mock::VerifyAndClearExpectations(&time_);
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(
      kNow + GetMisconnectsMonitorSeconds(), "")));
  EXPECT_FALSE(service_->HasRecentConnectionIssues());
  ASSERT_TRUE(GetMisconnects()->empty());
}

TEST_F(ServiceTest, NoteDisconnectEventMisconnectThreshold) {
  EXPECT_FALSE(service_->explicitly_disconnected());
  SetStateField(Service::kStateConfiguring);
  const int kNow = 8;
  for (int i = 0; i < GetReportMisconnectsThreshold() - 1; i++) {
    PushTimestamp(GetMisconnects(), kNow, "");
  }
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(kNow, "")));
  EXPECT_CALL(diagnostics_reporter_, OnConnectivityEvent()).Times(1);
  NoteDisconnectEvent();
  EXPECT_EQ(GetReportMisconnectsThreshold(), GetMisconnects()->size());
}

TEST_F(ServiceTest, NoteDisconnectEventDiscardOld) {
  EXPECT_FALSE(service_->explicitly_disconnected());
  EXPECT_CALL(diagnostics_reporter_, OnConnectivityEvent()).Times(0);
  for (int i = 0; i < 2; i++) {
    int now = 0;
    deque<Timestamp> *events = NULL;
    if (i == 0) {
      SetStateField(Service::kStateConnected);
      now = GetDisconnectsMonitorSeconds() + 1;
      events = GetDisconnects();
    } else {
      SetStateField(Service::kStateAssociating);
      now = GetMisconnectsMonitorSeconds() + 1;
      events = GetMisconnects();
    }
    PushTimestamp(events, 0, "");
    PushTimestamp(events, 0, "");
    EXPECT_CALL(time_, GetNow()).WillOnce(Return(GetTimestamp(now, "")));
    NoteDisconnectEvent();
    ASSERT_EQ(1, events->size());
    EXPECT_EQ(now, events->front().monotonic.tv_sec);
  }
}

TEST_F(ServiceTest, NoteDisconnectEventDiscardExcessive) {
  EXPECT_FALSE(service_->explicitly_disconnected());
  SetStateField(Service::kStateOnline);
  for (int i = 0; i < 2 * GetMaxDisconnectEventHistory(); i++) {
    PushTimestamp(GetDisconnects(), 0, "");
  }
  EXPECT_CALL(time_, GetNow()).WillOnce(Return(Timestamp()));
  EXPECT_CALL(diagnostics_reporter_, OnConnectivityEvent()).Times(1);
  NoteDisconnectEvent();
  EXPECT_EQ(GetMaxDisconnectEventHistory(), GetDisconnects()->size());
}

TEST_F(ServiceTest, ConvertTimestampsToStrings) {
  EXPECT_TRUE(ExtractWallClockToStrings(deque<Timestamp>()).empty());

  const Timestamp kValues[] = {
    GetTimestamp(123, "2012-12-09T12:41:22.123456+0100"),
    GetTimestamp(234, "2012-12-31T23:59:59.012345+0100")
  };
  Strings strings =
      ExtractWallClockToStrings(
          deque<Timestamp>(kValues, kValues + arraysize(kValues)));
  EXPECT_GT(arraysize(kValues), 0);
  ASSERT_EQ(arraysize(kValues), strings.size());
  for (size_t i = 0; i < arraysize(kValues); i++) {
    EXPECT_EQ(kValues[i].wall_clock, strings[i]);
  }
}

TEST_F(ServiceTest, DiagnosticsProperties) {
  const string kWallClock0 = "2012-12-09T12:41:22.234567-0800";
  const string kWallClock1 = "2012-12-31T23:59:59.345678-0800";
  Strings values;

  PushTimestamp(GetDisconnects(), 0, kWallClock0);
  Error unused_error;
  ASSERT_TRUE(service_->store().GetStringsProperty(
     kDiagnosticsDisconnectsProperty, &values, &unused_error));
  ASSERT_EQ(1, values.size());
  EXPECT_EQ(kWallClock0, values[0]);

  PushTimestamp(GetMisconnects(), 0, kWallClock1);
  ASSERT_TRUE(service_->store().GetStringsProperty(
      kDiagnosticsMisconnectsProperty, &values, &unused_error));
  ASSERT_EQ(1, values.size());
  EXPECT_EQ(kWallClock1, values[0]);
}

TEST_F(ServiceTest, SecurityLevel) {
  // Encrypted is better than not.
  service_->SetSecurity(Service::kCryptoNone, false, false);
  service2_->SetSecurity(Service::kCryptoRc4, false, false);
  EXPECT_GT(service2_->SecurityLevel(), service_->SecurityLevel());

  // AES encryption is better than RC4 encryption.
  service_->SetSecurity(Service::kCryptoRc4, false, false);
  service2_->SetSecurity(Service::kCryptoAes, false, false);
  EXPECT_GT(service2_->SecurityLevel(), service_->SecurityLevel());

  // Crypto algorithm is more important than key rotation.
  service_->SetSecurity(Service::kCryptoNone, true, false);
  service2_->SetSecurity(Service::kCryptoAes, false, false);
  EXPECT_GT(service2_->SecurityLevel(), service_->SecurityLevel());

  // Encrypted-but-unauthenticated is better than clear-but-authenticated.
  service_->SetSecurity(Service::kCryptoNone, false, true);
  service2_->SetSecurity(Service::kCryptoAes, false, false);
  EXPECT_GT(service2_->SecurityLevel(), service_->SecurityLevel());

  // For same encryption, prefer key rotation.
  service_->SetSecurity(Service::kCryptoRc4, false, false);
  service2_->SetSecurity(Service::kCryptoRc4, true, false);
  EXPECT_GT(service2_->SecurityLevel(), service_->SecurityLevel());

  // For same encryption, prefer authenticated AP.
  service_->SetSecurity(Service::kCryptoRc4, false, false);
  service2_->SetSecurity(Service::kCryptoRc4, false, true);
  EXPECT_GT(service2_->SecurityLevel(), service_->SecurityLevel());
}

TEST_F(ServiceTest, SetErrorDetails) {
  EXPECT_EQ(Service::kErrorDetailsNone, service_->error_details());
  static const char kDetails[] = "Certificate revoked.";
  ServiceMockAdaptor *adaptor = GetAdaptor();
  EXPECT_CALL(*adaptor, EmitStringChanged(kErrorDetailsProperty, kDetails));
  service_->SetErrorDetails(Service::kErrorDetailsNone);
  EXPECT_EQ(Service::kErrorDetailsNone, service_->error_details());
  service_->SetErrorDetails(kDetails);
  EXPECT_EQ(kDetails, service_->error_details());
  service_->SetErrorDetails(kDetails);
}

TEST_F(ServiceTest, SetAutoConnectFull) {
  EXPECT_FALSE(service_->auto_connect());
  Error error;
  EXPECT_FALSE(GetAutoConnect(&error));
  EXPECT_TRUE(error.IsSuccess());

  // false -> false
  EXPECT_FALSE(service_->retain_auto_connect());
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(0);
  SetAutoConnectFull(false, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_TRUE(service_->retain_auto_connect());
  EXPECT_FALSE(GetAutoConnect(NULL));
  Mock::VerifyAndClearExpectations(&mock_manager_);

  // Clear the |retain_auto_connect_| flag for the next test.
  service_->Unload();
  ASSERT_FALSE(service_->retain_auto_connect());

  // false -> true
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(1);
  SetAutoConnectFull(true, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(service_->auto_connect());
  EXPECT_TRUE(GetAutoConnect(NULL));
  EXPECT_TRUE(service_->retain_auto_connect());
  Mock::VerifyAndClearExpectations(&mock_manager_);

  // Clear the |retain_auto_connect_| flag for the next test.
  service_->Unload();
  ASSERT_FALSE(service_->retain_auto_connect());

  // true -> true
  service_->SetAutoConnect(true);
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(0);
  SetAutoConnectFull(true, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(service_->auto_connect());
  EXPECT_TRUE(GetAutoConnect(NULL));
  EXPECT_TRUE(service_->retain_auto_connect());
  Mock::VerifyAndClearExpectations(&mock_manager_);

  // Clear the |retain_auto_connect_| flag for the next test.
  service_->Unload();
  ASSERT_FALSE(service_->retain_auto_connect());

  // true -> false
  service_->SetAutoConnect(true);
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(1);
  SetAutoConnectFull(false, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_FALSE(GetAutoConnect(NULL));
  EXPECT_TRUE(service_->retain_auto_connect());
  Mock::VerifyAndClearExpectations(&mock_manager_);
}

TEST_F(ServiceTest, SetAutoConnectFullUserUpdatePersists) {
  // If the user sets the kAutoConnectProperty explicitly, the preference must
  // be persisted, even if the property was not changed.
  Error error;
  ServiceConstRefPtr service_ref(service_);
  MockProfileRefPtr mock_profile(
      new MockProfile(control_interface(), metrics(), &mock_manager_));
  NiceMock<MockStore> storage;
  service_->set_profile(mock_profile);
  service_->SetAutoConnect(true);

  EXPECT_CALL(*mock_profile, UpdateService(_));
  EXPECT_CALL(*mock_profile, GetConstStorage())
      .WillOnce(Return(&storage));
  EXPECT_CALL(mock_manager_, IsServiceEphemeral(service_ref))
      .WillOnce(Return(false));
  EXPECT_FALSE(service_->retain_auto_connect());
  SetAutoConnectFull(true, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(service_->auto_connect());
  EXPECT_TRUE(service_->retain_auto_connect());
}

TEST_F(ServiceTest, ClearAutoConnect) {
  EXPECT_FALSE(service_->auto_connect());
  Error error;
  EXPECT_FALSE(GetAutoConnect(&error));
  EXPECT_TRUE(error.IsSuccess());

  // unset -> false
  EXPECT_FALSE(service_->retain_auto_connect());
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(0);
  ClearAutoConnect(&error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_FALSE(service_->retain_auto_connect());
  EXPECT_FALSE(GetAutoConnect(NULL));
  Mock::VerifyAndClearExpectations(&mock_manager_);

  // false -> false
  SetAutoConnectFull(false, &error);
  EXPECT_FALSE(GetAutoConnect(NULL));
  EXPECT_TRUE(service_->retain_auto_connect());
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(0);
  ClearAutoConnect(&error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_FALSE(service_->retain_auto_connect());
  EXPECT_FALSE(GetAutoConnect(NULL));
  Mock::VerifyAndClearExpectations(&mock_manager_);

  // true -> false
  SetAutoConnectFull(true, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(GetAutoConnect(NULL));
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(1);
  ClearAutoConnect(&error);
  EXPECT_FALSE(service_->retain_auto_connect());
  EXPECT_FALSE(GetAutoConnect(NULL));
  Mock::VerifyAndClearExpectations(&mock_manager_);
}

TEST_F(ServiceTest, UniqueAttributes) {
  EXPECT_NE(service_->serial_number_, service2_->serial_number_);
  EXPECT_NE(service_->unique_name(), service2_->unique_name());
}

TEST_F(ServiceTest, PropertyChanges) {
  TestCommonPropertyChanges(service_, GetAdaptor());
  TestAutoConnectPropertyChange(service_, GetAdaptor());
}

// Custom property setters should return false, and make no changes, if
// the new value is the same as the old value.
TEST_F(ServiceTest, CustomSetterNoopChange) {
  TestCustomSetterNoopChange(service_, &mock_manager_);
}

TEST_F(ServiceTest, DHCPOptionFailureState) {
  // We are testing the transitions out of each node in this
  // state diagram:
  //
  //   [ Not Detected (send full request) ] <------------
  //         |                  ^                       |
  //         |                  |                       |
  //      n * failure        failure                    |
  //         |                  |                       |
  //         V                  |                       |
  //   [ Suspected (send minimal request) ]             |
  //                       |                            |
  //                    success                         |
  //                       |                            |
  //                       V                            |
  //   [ Confirmed (send minimal request) ]             |
  //         ^             |                            |
  //         |      hold timer elapsed                  |
  //         |             |                            |
  //      success          V                            |
  //         |     [ Retest Full Request ] ----success--/
  //         |             |          ^
  //         |          failure       |
  //         |             |          |
  //         |             V          |
  //   [ Retest Minimal Request ]     |
  //                       |       success
  //                    failure       |
  //                       |          |
  //                       V          |
  //                   [ Retest With No Reply (send minimal requests) ]

  // Check initial state.
  EXPECT_EQ(0, GetConsecutiveDHCPFailures());
  EXPECT_EQ(0, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureNotDetected,
            GetDHCPOptionFailureState());

  // Let's make up a constant to represent enough iterations that we
  // safely expect that the state machine won't transition as a result
  // of more iterations.
  const int kManyTimes = GetMaxDHCPOptionFailures() * 10;

  // NotDetected -> NotDetected.
  for (int i = 0; i < kManyTimes; ++i) {
    service_->OnDHCPSuccess();
    EXPECT_EQ(0, GetConsecutiveDHCPFailures());
    EXPECT_EQ(0, GetLastDHCPOptionFailure());
    EXPECT_EQ(Service::kDHCPOptionFailureNotDetected,
              GetDHCPOptionFailureState());
    EXPECT_FALSE(service_->ShouldUseMinimalDHCPConfig());
  }

  for (int i = 0; i < GetMaxDHCPOptionFailures() - 1; ++i) {
    service_->OnDHCPFailure();
    EXPECT_EQ(i + 1, GetConsecutiveDHCPFailures());
    EXPECT_EQ(0, GetLastDHCPOptionFailure());
    EXPECT_EQ(Service::kDHCPOptionFailureNotDetected,
              GetDHCPOptionFailureState());
    EXPECT_FALSE(service_->ShouldUseMinimalDHCPConfig());
  }

  NiceMock<MockStore> storage;
  EXPECT_CALL(storage,
              DeleteKey(_, StrNe(Service::kStorageLastDHCPOptionFailure)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(storage,
              DeleteKey(storage_id_, Service::kStorageLastDHCPOptionFailure));
  EXPECT_CALL(*eap_, Save(_, _, _)).Times(AnyNumber());
  EXPECT_TRUE(service_->Save(&storage));

  // NotDetected -> Suspected.
  service_->OnDHCPFailure();
  EXPECT_EQ(GetMaxDHCPOptionFailures(), GetConsecutiveDHCPFailures());
  // This value only updates at the time the failure is confirmed.
  EXPECT_EQ(0, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureSuspected, GetDHCPOptionFailureState());
  EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());

  EXPECT_CALL(storage,
              DeleteKey(storage_id_, Service::kStorageLastDHCPOptionFailure));
  EXPECT_TRUE(service_->Save(&storage));

  // Suspected -> NotDetected.
  EXPECT_EQ(GetMaxDHCPOptionFailures(), GetConsecutiveDHCPFailures());
  service_->OnDHCPFailure();
  EXPECT_EQ(0, GetConsecutiveDHCPFailures());
  EXPECT_EQ(0, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureNotDetected,
            GetDHCPOptionFailureState());
  EXPECT_FALSE(service_->ShouldUseMinimalDHCPConfig());

  EXPECT_CALL(storage,
              DeleteKey(storage_id_, Service::kStorageLastDHCPOptionFailure));
  EXPECT_TRUE(service_->Save(&storage));

  // Suspected -> Confirmed.
  SetConsecutiveDHCPFailures(GetMaxDHCPOptionFailures());
  SetLastDHCPOptionFailure(0);
  SetDHCPOptionFailureState(Service::kDHCPOptionFailureSuspected);

  int kFirstFailureTime = 1234;
  EXPECT_CALL(time_, GetNow())
      .WillRepeatedly(Return(GetTimestamp(kFirstFailureTime, "")));
  EXPECT_CALL(*metrics(), NotifyDHCPOptionFailure(_));
  service_->OnDHCPSuccess();
  Mock::VerifyAndClearExpectations(metrics());

  EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureConfirmed, GetDHCPOptionFailureState());
  EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());

  EXPECT_CALL(storage,
              SetUint64(storage_id_,
                        Service::kStorageLastDHCPOptionFailure,
                        kFirstFailureTime));
  EXPECT_TRUE(service_->Save(&storage));

  // Confirmed -> Confirmed.
  Mock::VerifyAndClearExpectations(&time_);
  int kAlmostAtFirstFailureExpiry =
      kFirstFailureTime + GetDHCPOptionHoldOffPeriodSeconds() - 1;
  EXPECT_CALL(time_, GetNow())
      .WillRepeatedly(Return(GetTimestamp(kAlmostAtFirstFailureExpiry, "")));

  for (int i = 0; i < kManyTimes; ++i) {
    service_->OnDHCPSuccess();
    EXPECT_EQ(0, GetConsecutiveDHCPFailures());
    EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
    EXPECT_EQ(Service::kDHCPOptionFailureConfirmed,
              GetDHCPOptionFailureState());
    EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());
  }

  for (int i = 0; i < kManyTimes; ++i) {
    service_->OnDHCPFailure();
    EXPECT_EQ(i + 1, GetConsecutiveDHCPFailures());
    EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
    EXPECT_EQ(Service::kDHCPOptionFailureConfirmed,
              GetDHCPOptionFailureState());
    EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());
  }

  EXPECT_CALL(storage,
              SetUint64(storage_id_,
                        Service::kStorageLastDHCPOptionFailure,
                        kFirstFailureTime));
  EXPECT_TRUE(service_->Save(&storage));

  // Confirmed -> RetestFullRequest.
  Mock::VerifyAndClearExpectations(&time_);
  int kFirstFailureExpiry =
      kFirstFailureTime + GetDHCPOptionHoldOffPeriodSeconds();
  EXPECT_CALL(time_, GetNow())
      .WillRepeatedly(Return(GetTimestamp(kFirstFailureExpiry, "")));
  EXPECT_FALSE(service_->ShouldUseMinimalDHCPConfig());
  EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureRetestFullRequest,
            GetDHCPOptionFailureState());

  EXPECT_CALL(storage,
              SetUint64(storage_id_,
                        Service::kStorageLastDHCPOptionFailure,
                        kFirstFailureTime));
  EXPECT_TRUE(service_->Save(&storage));

  // RetestFullRequest -> NotDetected.
  service_->OnDHCPSuccess();
  EXPECT_EQ(0, GetConsecutiveDHCPFailures());
  EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureNotDetected,
            GetDHCPOptionFailureState());
  EXPECT_FALSE(service_->ShouldUseMinimalDHCPConfig());

  EXPECT_CALL(storage,
              DeleteKey(storage_id_, Service::kStorageLastDHCPOptionFailure));
  EXPECT_TRUE(service_->Save(&storage));

  // RetestFullRequest -> RetestMinimalRequest.
  SetDHCPOptionFailureState(Service::kDHCPOptionFailureRetestFullRequest);

  service_->OnDHCPFailure();
  EXPECT_EQ(1, GetConsecutiveDHCPFailures());
  // This value only updates at the time the failure is confirmed.
  EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureRetestMinimalRequest,
            GetDHCPOptionFailureState());
  EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());

  EXPECT_CALL(storage,
              SetUint64(storage_id_,
                        Service::kStorageLastDHCPOptionFailure,
                        kFirstFailureTime));
  EXPECT_TRUE(service_->Save(&storage));

  // RetestMinimalRequest -> Confirmed.
  Mock::VerifyAndClearExpectations(&time_);
  int kSecondFailureTime = kFirstFailureExpiry + 1;
  EXPECT_CALL(time_, GetNow())
      .WillRepeatedly(Return(GetTimestamp(kSecondFailureTime, "")));
  EXPECT_CALL(*metrics(), NotifyDHCPOptionFailure(_));
  service_->OnDHCPSuccess();
  Mock::VerifyAndClearExpectations(metrics());

  EXPECT_EQ(0, GetConsecutiveDHCPFailures());
  EXPECT_EQ(kSecondFailureTime, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureConfirmed, GetDHCPOptionFailureState());
  EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());

  EXPECT_CALL(storage,
              SetUint64(storage_id_,
                        Service::kStorageLastDHCPOptionFailure,
                        kSecondFailureTime));
  EXPECT_TRUE(service_->Save(&storage));

  // RetestMinimalRequest -> RetestGotNoReply.
  SetDHCPOptionFailureState(Service::kDHCPOptionFailureRetestMinimalRequest);
  SetConsecutiveDHCPFailures(1);
  SetLastDHCPOptionFailure(kFirstFailureTime);

  service_->OnDHCPFailure();
  EXPECT_EQ(2, GetConsecutiveDHCPFailures());
  EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureRetestGotNoReply,
            GetDHCPOptionFailureState());
  EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());

  EXPECT_CALL(storage,
              SetUint64(storage_id_,
                        Service::kStorageLastDHCPOptionFailure,
                        kFirstFailureTime));
  EXPECT_TRUE(service_->Save(&storage));

  // RetestGotNoReply -> RetestGotNoReply.
  for (int i = 0; i < kManyTimes; ++i) {
    service_->OnDHCPFailure();
    EXPECT_EQ(i + 3, GetConsecutiveDHCPFailures());
    EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
    EXPECT_EQ(Service::kDHCPOptionFailureRetestGotNoReply,
              GetDHCPOptionFailureState());
    EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());
  }

  EXPECT_CALL(storage,
              SetUint64(storage_id_,
                        Service::kStorageLastDHCPOptionFailure,
                        kFirstFailureTime));
  EXPECT_TRUE(service_->Save(&storage));

  // RetestGotNoReply -> RetestFullRequest.
  service_->OnDHCPSuccess();
  EXPECT_EQ(0, GetConsecutiveDHCPFailures());
  EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureRetestFullRequest,
            GetDHCPOptionFailureState());
  EXPECT_FALSE(service_->ShouldUseMinimalDHCPConfig());

  EXPECT_CALL(storage,
              SetUint64(storage_id_,
                        Service::kStorageLastDHCPOptionFailure,
                        kFirstFailureTime));
  EXPECT_TRUE(service_->Save(&storage));

  // Load into NotDetected.
  EXPECT_CALL(storage, ContainsGroup(storage_id_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*eap_, Load(&storage, storage_id_)).Times(AnyNumber());
  EXPECT_CALL(storage,
              GetUint64(storage_id_, Service::kStorageLastDHCPOptionFailure, _))
      .WillOnce(Return(false));
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_EQ(0, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureNotDetected,
            GetDHCPOptionFailureState());
  EXPECT_FALSE(service_->ShouldUseMinimalDHCPConfig());
  // Note that ShouldUseMinimalDHCPConfig() doesn't change this state.
  EXPECT_EQ(Service::kDHCPOptionFailureNotDetected,
            GetDHCPOptionFailureState());

  // Load into Confirmed.
  EXPECT_CALL(storage,
              GetUint64(storage_id_, Service::kStorageLastDHCPOptionFailure, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kSecondFailureTime), Return(true)));
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_EQ(kSecondFailureTime, GetLastDHCPOptionFailure());
  EXPECT_EQ(Service::kDHCPOptionFailureConfirmed, GetDHCPOptionFailureState());
  EXPECT_TRUE(service_->ShouldUseMinimalDHCPConfig());
  // Note that ShouldUseMinimalDHCPConfig() doesn't change this state.
  EXPECT_EQ(Service::kDHCPOptionFailureConfirmed, GetDHCPOptionFailureState());

  // Load into RetestFullRequest.
  EXPECT_CALL(storage,
              GetUint64(storage_id_, Service::kStorageLastDHCPOptionFailure, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kFirstFailureTime), Return(true)));
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_EQ(kFirstFailureTime, GetLastDHCPOptionFailure());
  // At load time we believe we're confirmed...
  EXPECT_EQ(Service::kDHCPOptionFailureConfirmed, GetDHCPOptionFailureState());
  // But as soon as we query ShouldUseMinimalConfig, we'll switch, since
  // kFirstFailureTime is too far in the past.
  EXPECT_FALSE(service_->ShouldUseMinimalDHCPConfig());
  EXPECT_EQ(Service::kDHCPOptionFailureRetestFullRequest,
            GetDHCPOptionFailureState());
}

TEST_F(ServiceTest, GetTethering) {
  Error error;
  EXPECT_EQ("", service_->GetTethering(&error));
  EXPECT_EQ(Error::kNotSupported, error.type());
}

}  // namespace shill
