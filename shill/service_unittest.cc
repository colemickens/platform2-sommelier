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
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_log.h"
#include "shill/mock_manager.h"
#include "shill/mock_power_manager.h"
#include "shill/mock_profile.h"
#include "shill/mock_proxy_factory.h"
#include "shill/mock_store.h"
#include "shill/mock_time.h"
#include "shill/property_store_unittest.h"
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
        storage_id_(ServiceUnderTest::kStorageId),
        power_manager_(new MockPowerManager(NULL, &proxy_factory_)) {
    service_->time_ = &time_;
    DefaultValue<Timestamp>::Set(Timestamp());
    service_->diagnostics_reporter_ = &diagnostics_reporter_;
    mock_manager_.running_ = true;
    mock_manager_.set_power_manager(power_manager_);  // Passes ownership.
  }

  virtual ~ServiceTest() {}

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  typedef scoped_refptr<MockProfile> MockProfileRefPtr;

  class TestProxyFactory : public ProxyFactory {
   public:
    TestProxyFactory() {}

    virtual PowerManagerProxyInterface *CreatePowerManagerProxy(
        PowerManagerProxyDelegate *delegate) {
      return NULL;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(TestProxyFactory);
  };

  string GetFriendlyName() { return service_->friendly_name(); }

  void SetManagerRunning(bool running) { mock_manager_.running_ = running; }

  void SetPowerState(PowerManager::SuspendState state) {
    power_manager_->power_state_ = state;
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

  MockManager mock_manager_;
  MockDiagnosticsReporter diagnostics_reporter_;
  MockTime time_;
  scoped_refptr<ServiceUnderTest> service_;
  string storage_id_;
  TestProxyFactory proxy_factory_;
  MockPowerManager *power_manager_;  // Owned by |mock_manager_|.
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
}

TEST_F(ServiceTest, CalculateState) {
  service_->state_ = Service::kStateConnected;
  Error error;
  EXPECT_EQ(flimflam::kStateReady, service_->CalculateState(&error));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(ServiceTest, CalculateTechnology) {
  service_->technology_ = Technology::kWifi;
  Error error;
  EXPECT_EQ(flimflam::kTypeWifi, service_->CalculateTechnology(&error));
  EXPECT_TRUE(error.IsSuccess());
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
                                             GetFriendlyName()),
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
  EXPECT_CALL(storage, GetBool(storage_id_, _, _)).Times(AnyNumber());
  EXPECT_CALL(storage,
              GetBool(storage_id_, Service::kStorageSaveCredentials, _));
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
  EXPECT_CALL(storage, SetBool(storage_id_, _, _)).Times(AnyNumber());
  EXPECT_CALL(storage,
              SetBool(storage_id_,
                      Service::kStorageSaveCredentials,
                      service_->save_credentials()));
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
  ASSERT_TRUE(service_->Load(&storage));
  // TODO(pstew): Only two string properties in the service are tested as
  // a sentinel that properties are being set and reset at the right times.
  // However, since property load/store is essentially a manual process,
  // it is error prone and should either be exhaustively unit-tested or
  // a generic framework for registering loaded/stored properties should
  // be created. crosbug.com/24859
  EXPECT_EQ(string_value, service_->ui_data_);
  EXPECT_EQ(string_value, service_->guid_);
  EXPECT_FALSE(service_->explicitly_disconnected_);
  EXPECT_TRUE(service_->has_ever_connected_);
  service_->explicitly_disconnected_ = true;
  service_->Unload();
  EXPECT_EQ(string(""), service_->ui_data_);
  EXPECT_EQ(string(""), service_->guid_);
  EXPECT_FALSE(service_->explicitly_disconnected_);
  EXPECT_FALSE(service_->has_ever_connected_);
}

TEST_F(ServiceTest, State) {
  EXPECT_EQ(Service::kStateIdle, service_->state());
  EXPECT_EQ(Service::kStateIdle, GetPreviousState());
  EXPECT_EQ(Service::kFailureUnknown, service_->failure());
  const string unknown_error(
      Service::ConnectFailureToString(Service::kFailureUnknown));
  EXPECT_EQ(unknown_error, service_->error());

  ServiceRefPtr service_ref(service_);

  EXPECT_CALL(*dynamic_cast<ServiceMockAdaptor *>(service_->adaptor_.get()),
              EmitStringChanged(flimflam::kStateProperty, _)).Times(7);
  EXPECT_CALL(*dynamic_cast<ServiceMockAdaptor *>(service_->adaptor_.get()),
              EmitStringChanged(flimflam::kErrorProperty, _)).Times(4);
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
  service_->SetState(Service::kStateDisconnected);
  EXPECT_EQ(Service::kStateConnected, GetPreviousState());

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetFailure(Service::kFailureOutOfRange);
  EXPECT_TRUE(service_->IsFailed());
  EXPECT_GT(service_->failed_time_, 0);
  EXPECT_EQ(Service::kStateFailure, service_->state());
  EXPECT_EQ(Service::kFailureOutOfRange, service_->failure());
  const string out_of_range_error(
      Service::ConnectFailureToString(Service::kFailureOutOfRange));
  EXPECT_EQ(out_of_range_error, service_->error());

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetState(Service::kStateConnected);
  EXPECT_FALSE(service_->IsFailed());
  EXPECT_EQ(service_->failed_time_, 0);
  EXPECT_EQ(unknown_error, service_->error());

  EXPECT_CALL(mock_manager_, UpdateService(service_ref));
  service_->SetFailureSilent(Service::kFailurePinMissing);
  EXPECT_TRUE(service_->IsFailed());
  EXPECT_GT(service_->failed_time_, 0);
  EXPECT_EQ(Service::kStateIdle, service_->state());
  EXPECT_EQ(Service::kFailurePinMissing, service_->failure());
  const string pin_missing_error(
      Service::ConnectFailureToString(Service::kFailurePinMissing));
  EXPECT_EQ(pin_missing_error, service_->error());

  // If the Service has a Profile, the profile should be saved when
  // the service enters kStateConnected. (The case where the service
  // doesn't have a profile is tested above.)
  MockProfileRefPtr mock_profile(
      new MockProfile(control_interface(), &mock_manager_));
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
  const char *reason = NULL;
  service_->set_connectable(true);

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
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // A deliberate Connect should also re-enable auto-connect.
  service_->UserInitiatedDisconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  service_->Connect(&error);
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
  service_->set_connectable(true);

  ScopeLogger::GetInstance()->EnableScopesByName("+service");
  ScopeLogger::GetInstance()->set_verbose_level(1);
  service_->SetState(Service::kStateConnected);
  EXPECT_CALL(log, Log(-1, _, HasSubstr(Service::kAutoConnConnected)));
  service_->AutoConnect();

  ScopeLogger::GetInstance()->EnableScopesByName("-service");
  ScopeLogger::GetInstance()->set_verbose_level(0);
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       HasSubstr(Service::kAutoConnNotConnectable)));
  service_->set_connectable(false);
  service_->AutoConnect();
}


TEST_F(AllMockServiceTest, AutoConnectWithFailures) {
  const char *reason;
  service_->set_connectable(true);
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

TEST_F(ServiceTest, ConfigureIntProperty) {
  const int kPriority0 = 100;
  const int kPriority1 = 200;
  service_->set_priority(kPriority0);
  ASSERT_EQ(kPriority0, service_->priority());
  KeyValueStore args;
  args.SetInt(flimflam::kPriorityProperty, kPriority1);
  Error error;
  service_->Configure(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kPriority1, service_->priority());
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

TEST_F(ServiceTest, DoPropertiesMatch) {
  service_->set_auto_connect(false);
  const string kGUID0 = "guid_zero";
  const string kGUID1 = "guid_one";
  service_->set_guid(kGUID0);
  const uint32 kPriority0 = 100;
  const uint32 kPriority1 = 200;
  service_->set_priority(kPriority0);

  {
    KeyValueStore args;
    args.SetString(flimflam::kGuidProperty, kGUID0);
    args.SetBool(flimflam::kAutoConnectProperty, false);
    args.SetInt(flimflam::kPriorityProperty, kPriority0);
    EXPECT_TRUE(service_->DoPropertiesMatch(args));
  }
  {
    KeyValueStore args;
    args.SetString(flimflam::kGuidProperty, kGUID1);
    args.SetBool(flimflam::kAutoConnectProperty, false);
    args.SetInt(flimflam::kPriorityProperty, kPriority0);
    EXPECT_FALSE(service_->DoPropertiesMatch(args));
  }
  {
    KeyValueStore args;
    args.SetString(flimflam::kGuidProperty, kGUID0);
    args.SetBool(flimflam::kAutoConnectProperty, true);
    args.SetInt(flimflam::kPriorityProperty, kPriority0);
    EXPECT_FALSE(service_->DoPropertiesMatch(args));
  }
  {
    KeyValueStore args;
    args.SetString(flimflam::kGuidProperty, kGUID0);
    args.SetBool(flimflam::kAutoConnectProperty, false);
    args.SetInt(flimflam::kPriorityProperty, kPriority1);
    EXPECT_FALSE(service_->DoPropertiesMatch(args));
  }
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
  EXPECT_CALL(*mock_connection0.get(), GetLowerConnection())
      .WillRepeatedly(Return(mock_connection1));
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

  service_->connection_ = NULL;
  service1->connection_ = NULL;
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

TEST_F(ServiceTest, SetFriendlyName) {
  EXPECT_EQ(service_->unique_name_, service_->friendly_name_);
  ServiceMockAdaptor *adaptor =
      dynamic_cast<ServiceMockAdaptor *>(service_->adaptor());

  EXPECT_CALL(*adaptor, EmitStringChanged(_, _)).Times(0);
  service_->SetFriendlyName(service_->unique_name_);
  EXPECT_EQ(service_->unique_name_, service_->friendly_name_);

  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kNameProperty,
                                          "Test Name 1"));
  service_->SetFriendlyName("Test Name 1");
  EXPECT_EQ("Test Name 1", service_->friendly_name_);

  EXPECT_CALL(*adaptor, EmitStringChanged(_, _)).Times(0);
  service_->SetFriendlyName("Test Name 1");
  EXPECT_EQ("Test Name 1", service_->friendly_name_);

  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kNameProperty,
                                          "Test Name 2"));
  service_->SetFriendlyName("Test Name 2");
  EXPECT_EQ("Test Name 2", service_->friendly_name_);
}

TEST_F(ServiceTest, SetConnectable) {
  EXPECT_FALSE(service_->connectable());

  ServiceMockAdaptor *adaptor =
      dynamic_cast<ServiceMockAdaptor *>(service_->adaptor());

  EXPECT_CALL(*adaptor, EmitBoolChanged(_, _)).Times(0);
  EXPECT_CALL(mock_manager_, HasService(_)).Times(0);
  service_->SetConnectable(false);
  EXPECT_FALSE(service_->connectable());

  EXPECT_CALL(*adaptor, EmitBoolChanged(flimflam::kConnectableProperty, true));
  EXPECT_CALL(mock_manager_, HasService(_)).WillOnce(Return(false));
  EXPECT_CALL(mock_manager_, UpdateService(_)).Times(0);
  service_->SetConnectable(true);
  EXPECT_TRUE(service_->connectable());

  EXPECT_CALL(*adaptor, EmitBoolChanged(flimflam::kConnectableProperty, false));
  EXPECT_CALL(mock_manager_, HasService(_)).WillOnce(Return(true));
  EXPECT_CALL(mock_manager_, UpdateService(_));
  service_->SetConnectable(false);
  EXPECT_FALSE(service_->connectable());

  EXPECT_CALL(*adaptor, EmitBoolChanged(flimflam::kConnectableProperty, true));
  EXPECT_CALL(mock_manager_, HasService(_)).WillOnce(Return(true));
              EXPECT_CALL(mock_manager_, UpdateService(_));
  service_->SetConnectable(true);
  EXPECT_TRUE(service_->connectable());
}

// Make sure a property is registered as a write only property
// by reading and comparing all string properties returned on the store.
// Subtle: We need to convert the test argument back and forth between
// string and ::DBus::Variant because this is the parameter type that
// our supeclass (PropertyStoreTest) is declared with.
class ReadOnlyServicePropertyTest : public ServiceTest {};
TEST_P(ReadOnlyServicePropertyTest, PropertyWriteOnly) {
  string property(GetParam().reader().get_string());
  Error error;
  EXPECT_FALSE(service_->store().GetStringProperty(property, NULL, &error));
  EXPECT_EQ(Error::kPermissionDenied, error.type());
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

class ServiceWithMockSetEap : public ServiceUnderTest {
 public:
  ServiceWithMockSetEap(ControlInterface *control_interface,
                        EventDispatcher *dispatcher,
                        Metrics *metrics,
                        Manager *manager)
      : ServiceUnderTest(control_interface, dispatcher, metrics, manager),
        is_8021x_(false) {}
  MOCK_METHOD1(set_eap, void(const EapCredentials &eap));
  virtual bool Is8021x() const { return is_8021x_; }
  void set_is_8021x(bool is_8021x) { is_8021x_ = is_8021x; }

 private:
  bool is_8021x_;
};

TEST_F(ServiceTest, SetEAPCredentialsOverRPC) {
  scoped_refptr<ServiceWithMockSetEap> service(
      new ServiceWithMockSetEap(control_interface(),
                                dispatcher(),
                                metrics(),
                                &mock_manager_));
  string eap_credential_properties[] = {
      flimflam::kEAPCertIDProperty,
      flimflam::kEAPClientCertProperty,
      flimflam::kEAPKeyIDProperty,
      flimflam::kEAPPINProperty,
      flimflam::kEapCaCertIDProperty,
      flimflam::kEapIdentityProperty,
      flimflam::kEapPasswordProperty,
      flimflam::kEapPrivateKeyProperty
  };
  string eap_non_credential_properties[] = {
      flimflam::kEAPEAPProperty,
      flimflam::kEapPhase2AuthProperty,
      flimflam::kEapAnonymousIdentityProperty,
      flimflam::kEapPrivateKeyPasswordProperty,
      flimflam::kEapCaCertNssProperty,
      flimflam::kEapUseSystemCAsProperty
  };
  // While this is not an 802.1x-based service, none of these property
  // changes should cause a call to set_eap().
  EXPECT_CALL(*service, set_eap(_)).Times(0);
  for (size_t i = 0; i < arraysize(eap_credential_properties); ++i)
    service->OnPropertyChanged(eap_credential_properties[i]);
  for (size_t i = 0; i < arraysize(eap_non_credential_properties); ++i)
    service->OnPropertyChanged(eap_non_credential_properties[i]);
  service->OnPropertyChanged(flimflam::kEapKeyMgmtProperty);

  service->set_is_8021x(true);

  // When this is an 802.1x-based service, set_eap should be called for
  // all credential-carrying properties.
  for (size_t i = 0; i < arraysize(eap_credential_properties); ++i) {
    EXPECT_CALL(*service, set_eap(_)).Times(1);
    service->OnPropertyChanged(eap_credential_properties[i]);
    Mock::VerifyAndClearExpectations(service.get());
  }

  // The key management property is a special case.  While not strictly
  // a credential, it can change which credentials are used.  Therefore it
  // should also trigger a call to set_eap();
  EXPECT_CALL(*service, set_eap(_)).Times(1);
  service->OnPropertyChanged(flimflam::kEapKeyMgmtProperty);
  Mock::VerifyAndClearExpectations(service.get());

  EXPECT_CALL(*service, set_eap(_)).Times(0);
  for (size_t i = 0; i < arraysize(eap_non_credential_properties); ++i)
    service->OnPropertyChanged(eap_non_credential_properties[i]);
}

TEST_F(ServiceTest, Certification) {
  EXPECT_FALSE(service_->eap_.remote_certification.size());

  ScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_WARNING, _,
                       HasSubstr("exceeds our maximum"))).Times(2);
  string kSubject("foo");
  EXPECT_FALSE(service_->AddEAPCertification(
      kSubject, Service::kEAPMaxCertificationElements));
  EXPECT_FALSE(service_->AddEAPCertification(
      kSubject, Service::kEAPMaxCertificationElements + 1));
  EXPECT_FALSE(service_->eap_.remote_certification.size());
  Mock::VerifyAndClearExpectations(&log);

  EXPECT_CALL(log,
              Log(logging::LOG_INFO, _, HasSubstr("Received certification")))
      .Times(1);
  EXPECT_TRUE(service_->AddEAPCertification(
      kSubject, Service::kEAPMaxCertificationElements - 1));
  Mock::VerifyAndClearExpectations(&log);
  EXPECT_EQ(Service::kEAPMaxCertificationElements,
      service_->eap_.remote_certification.size());
  for (size_t i = 0; i < Service::kEAPMaxCertificationElements - 1; ++i) {
      EXPECT_TRUE(service_->eap_.remote_certification[i].empty());
  }
  EXPECT_EQ(kSubject, service_->eap_.remote_certification[
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
  SetPowerState(PowerManager::kSuspending);
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

}  // namespace shill
