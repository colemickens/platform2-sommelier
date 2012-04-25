// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <map>
#include <set>

#include <glib.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/scoped_temp_dir.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/adaptor_interfaces.h"
#include "shill/ephemeral_profile.h"
#include "shill/error.h"
#include "shill/glib.h"
#include "shill/key_file_store.h"
#include "shill/key_value_store.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_connection.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_device_info.h"
#include "shill/mock_glib.h"
#include "shill/mock_metrics.h"
#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/mock_wifi.h"
#include "shill/mock_wifi_service.h"
#include "shill/property_store_unittest.h"
#include "shill/service_under_test.h"
#include "shill/wifi_service.h"
#include "shill/wimax_service.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Test;

class ManagerTest : public PropertyStoreTest {
 public:
  ManagerTest()
      : mock_wifi_(new NiceMock<MockWiFi>(control_interface(),
                                          dispatcher(),
                                          metrics(),
                                          manager(),
                                          "wifi0",
                                          "addr4",
                                          4)),
        device_info_(new NiceMock<MockDeviceInfo>(
            control_interface(),
            reinterpret_cast<EventDispatcher*>(NULL),
            reinterpret_cast<Metrics*>(NULL),
            reinterpret_cast<Manager*>(NULL))),
        manager_adaptor_(new NiceMock<ManagerMockAdaptor>()) {
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     metrics(),
                                                     manager(),
                                                     "null0",
                                                     "addr0",
                                                     0));
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     metrics(),
                                                     manager(),
                                                     "null1",
                                                     "addr1",
                                                     1));
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     metrics(),
                                                     manager(),
                                                     "null2",
                                                     "addr2",
                                                     2));
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     metrics(),
                                                     manager(),
                                                     "null3",
                                                     "addr3",
                                                     3));
    manager()->connect_profiles_to_rpc_ = false;

    // Replace the manager's adaptor with a quieter one, and one
    // we can do EXPECT*() against.  Passes ownership.
    manager()->adaptor_.reset(manager_adaptor_);
  }
  virtual ~ManagerTest() {}

  bool IsDeviceRegistered(const DeviceRefPtr &device,
                          Technology::Identifier tech) {
    vector<DeviceRefPtr> devices;
    manager()->FilterByTechnology(tech, &devices);
    return (devices.size() == 1 && devices[0].get() == device.get());
  }
  bool ServiceOrderIs(ServiceRefPtr svc1, ServiceRefPtr svc2);

  void AdoptProfile(Manager *manager, ProfileRefPtr profile) {
    manager->profiles_.push_back(profile);
  }

  ProfileRefPtr GetEphemeralProfile(Manager *manager) {
    return manager->ephemeral_profile_;
  }

  Profile *CreateProfileForManager(Manager *manager, GLib *glib) {
    Profile::Identifier id("rather", "irrelevant");
    scoped_ptr<Profile> profile(new Profile(control_interface(),
                                            manager,
                                            id,
                                            "",
                                            false));
    FilePath final_path(storage_path());
    final_path = final_path.Append("test.profile");
    scoped_ptr<KeyFileStore> storage(new KeyFileStore(glib));
    storage->set_path(final_path);
    if (!storage->Open())
      return NULL;
    profile->set_storage(storage.release());  // Passes ownership.
    return profile.release();
  }

  bool CreateBackingStoreForService(ScopedTempDir *temp_dir,
                                    const string &profile_identifier,
                                    const string &service_name) {
    GLib glib;
    KeyFileStore store(&glib);
    store.set_path(temp_dir->path().Append(profile_identifier + ".profile"));
    return store.Open() &&
        store.SetString(service_name, "rather", "irrelevant") &&
        store.Close();
  }

  Error::Type TestCreateProfile(Manager *manager, const string &name) {
    Error error;
    string path;
    manager->CreateProfile(name, &path, &error);
    return error.type();
  }

  Error::Type TestPopAnyProfile(Manager *manager) {
    Error error;
    manager->PopAnyProfile(&error);
    return error.type();
  }

  Error::Type TestPopProfile(Manager *manager, const string &name) {
    Error error;
    manager->PopProfile(name, &error);
    return error.type();
  }

  Error::Type TestPushProfile(Manager *manager, const string &name) {
    Error error;
    string path;
    manager->PushProfile(name, &path, &error);
    return error.type();
  }

  void AddMockProfileToManager(Manager *manager) {
    scoped_refptr<MockProfile> profile(
        new MockProfile(control_interface(), manager, ""));
    EXPECT_CALL(*profile, GetRpcIdentifier())
        .WillRepeatedly(Return("/"));
    AdoptProfile(manager, profile);
  }

  void CompleteServiceSort() {
    EXPECT_FALSE(manager()->sort_services_task_.IsCancelled());
    dispatcher()->DispatchPendingEvents();
    EXPECT_TRUE(manager()->sort_services_task_.IsCancelled());
  }

 protected:
  typedef scoped_refptr<MockService> MockServiceRefPtr;

  MockServiceRefPtr MakeAutoConnectableService() {
    MockServiceRefPtr service = new NiceMock<MockService>(control_interface(),
                                                          dispatcher(),
                                                          metrics(),
                                                          manager());
    service->MakeFavorite();
    service->set_connectable(true);
    return service;
  }

  scoped_refptr<MockWiFi> mock_wifi_;
  vector<scoped_refptr<MockDevice> > mock_devices_;
  scoped_ptr<MockDeviceInfo> device_info_;

  // This pointer is owned by the manager, and only tracked here for EXPECT*()
  ManagerMockAdaptor *manager_adaptor_;
};

bool ManagerTest::ServiceOrderIs(ServiceRefPtr svc0, ServiceRefPtr svc1) {
  if (!manager()->sort_services_task_.IsCancelled()) {
    manager()->SortServicesTask();
  }
  return (svc0.get() == manager()->services_[0].get() &&
          svc1.get() == manager()->services_[1].get());
}

TEST_F(ManagerTest, Contains) {
  EXPECT_TRUE(manager()->store().Contains(flimflam::kStateProperty));
  EXPECT_FALSE(manager()->store().Contains(""));
}

TEST_F(ManagerTest, DeviceRegistration) {
  ON_CALL(*mock_devices_[0].get(), technology())
      .WillByDefault(Return(Technology::kEthernet));
  ON_CALL(*mock_devices_[1].get(), technology())
      .WillByDefault(Return(Technology::kWifi));
  ON_CALL(*mock_devices_[2].get(), technology())
      .WillByDefault(Return(Technology::kCellular));

  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  manager()->RegisterDevice(mock_devices_[2]);

  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));
  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));
  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[2], Technology::kCellular));
}

TEST_F(ManagerTest, DeviceRegistrationAndStart) {
  manager()->running_ = true;
  mock_devices_[0]->enabled_persistent_ = true;
  mock_devices_[1]->enabled_persistent_ = false;
  EXPECT_CALL(*mock_devices_[0].get(), SetEnabled(true))
      .Times(1);
  EXPECT_CALL(*mock_devices_[1].get(), SetEnabled(_))
      .Times(0);
  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
}

TEST_F(ManagerTest, DeviceRegistrationWithProfile) {
  MockProfile *profile = new MockProfile(control_interface(), manager(), "");
  DeviceRefPtr device_ref(mock_devices_[0].get());
  AdoptProfile(manager(), profile);  // Passes ownership.
  EXPECT_CALL(*profile, ConfigureDevice(device_ref));
  EXPECT_CALL(*profile, Save());
  manager()->RegisterDevice(mock_devices_[0]);
}

TEST_F(ManagerTest, DeviceDeregistration) {
  ON_CALL(*mock_devices_[0].get(), technology())
      .WillByDefault(Return(Technology::kEthernet));
  ON_CALL(*mock_devices_[1].get(), technology())
      .WillByDefault(Return(Technology::kWifi));

  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);

  ASSERT_TRUE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));
  ASSERT_TRUE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));

  EXPECT_CALL(*mock_devices_[0].get(), SetEnabled(false));
  manager()->DeregisterDevice(mock_devices_[0]);
  EXPECT_FALSE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));

  EXPECT_CALL(*mock_devices_[1].get(), SetEnabled(false));
  manager()->DeregisterDevice(mock_devices_[1]);
  EXPECT_FALSE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));
}

TEST_F(ManagerTest, ServiceRegistration) {
  // It's much easier and safer to use a real GLib for this test.
  GLib glib;
  Manager manager(control_interface(),
                  dispatcher(),
                  metrics(),
                  &glib,
                  run_path(),
                  storage_path(),
                  string());
  ProfileRefPtr profile(CreateProfileForManager(&manager, &glib));
  ASSERT_TRUE(profile.get());
  AdoptProfile(&manager, profile);

  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                &manager));
  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                &manager));

  string service1_name(mock_service->UniqueName());
  string service2_name(mock_service2->UniqueName());

  EXPECT_CALL(*mock_service.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(service1_name));
  EXPECT_CALL(*mock_service2.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(service2_name));
  // TODO(quiche): make this EXPECT_CALL work (crosbug.com/20154)
  // EXPECT_CALL(*dynamic_cast<ManagerMockAdaptor *>(manager.adaptor_.get()),
  //             EmitRpcIdentifierArrayChanged(flimflam::kServicesProperty, _));

  manager.RegisterService(mock_service);
  manager.RegisterService(mock_service2);

  Error error;
  vector<string> rpc_ids = manager.EnumerateAvailableServices(&error);
  set<string> ids(rpc_ids.begin(), rpc_ids.end());
  EXPECT_EQ(2, ids.size());
  EXPECT_TRUE(ContainsKey(ids, mock_service->GetRpcIdentifier()));
  EXPECT_TRUE(ContainsKey(ids, mock_service2->GetRpcIdentifier()));

  EXPECT_TRUE(manager.FindService(service1_name).get() != NULL);
  EXPECT_TRUE(manager.FindService(service2_name).get() != NULL);

  manager.Stop();
}

TEST_F(ManagerTest, RegisterKnownService) {
  // It's much easier and safer to use a real GLib for this test.
  GLib glib;
  Manager manager(control_interface(),
                  dispatcher(),
                  metrics(),
                  &glib,
                  run_path(),
                  storage_path(),
                  string());
  ProfileRefPtr profile(CreateProfileForManager(&manager, &glib));
  ASSERT_TRUE(profile.get());
  AdoptProfile(&manager, profile);
  {
    ServiceRefPtr service1(new ServiceUnderTest(control_interface(),
                                                dispatcher(),
                                                metrics(),
                                                &manager));
    ASSERT_TRUE(profile->AdoptService(service1));
    ASSERT_TRUE(profile->ContainsService(service1));
  }  // Force destruction of service1.

  ServiceRefPtr service2(new ServiceUnderTest(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              &manager));
  manager.RegisterService(service2);
  EXPECT_EQ(service2->profile().get(), profile.get());
  manager.Stop();
}

TEST_F(ManagerTest, RegisterUnknownService) {
  // It's much easier and safer to use a real GLib for this test.
  GLib glib;
  Manager manager(control_interface(),
                  dispatcher(),
                  metrics(),
                  &glib,
                  run_path(),
                  storage_path(),
                  string());
  ProfileRefPtr profile(CreateProfileForManager(&manager, &glib));
  ASSERT_TRUE(profile.get());
  AdoptProfile(&manager, profile);
  {
    ServiceRefPtr service1(new ServiceUnderTest(control_interface(),
                                                dispatcher(),
                                                metrics(),
                                                &manager));
    ASSERT_TRUE(profile->AdoptService(service1));
    ASSERT_TRUE(profile->ContainsService(service1));
  }  // Force destruction of service1.
  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                &manager));
  EXPECT_CALL(*mock_service2.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(mock_service2->UniqueName()));
  manager.RegisterService(mock_service2);
  EXPECT_NE(mock_service2->profile().get(), profile.get());
  manager.Stop();
}

TEST_F(ManagerTest, DeregisterUnregisteredService) {
  // WiFi assumes that it can deregister a service that is not
  // registered.  (E.g. a hidden service can be deregistered when it
  // loses its last endpoint, and again when WiFi is Stop()-ed.)
  //
  // So test that doing so doesn't cause a crash.
  MockServiceRefPtr service = new NiceMock<MockService>(control_interface(),
                                                        dispatcher(),
                                                        metrics(),
                                                        manager());
  manager()->DeregisterService(service);
}

TEST_F(ManagerTest, GetProperties) {
  AddMockProfileToManager(manager());
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    string expected("portal_list");
    manager()->mutable_store()->SetStringProperty(
        flimflam::kCheckPortalListProperty,
        expected,
        &error);
    DBusAdaptor::GetProperties(manager()->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kCheckPortalListProperty) == props.end());
    EXPECT_EQ(props[flimflam::kCheckPortalListProperty].reader().get_string(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    bool expected = true;
    manager()->mutable_store()->SetBoolProperty(flimflam::kOfflineModeProperty,
                                                expected,
                                                &error);
    DBusAdaptor::GetProperties(manager()->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kOfflineModeProperty) == props.end());
    EXPECT_EQ(props[flimflam::kOfflineModeProperty].reader().get_bool(),
              expected);
  }
}

TEST_F(ManagerTest, GetDevicesProperty) {
  AddMockProfileToManager(manager());
  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  {
    map<string, ::DBus::Variant> props;
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(manager()->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kDevicesProperty) == props.end());
    vector < ::DBus::Path> devices =
        props[flimflam::kDevicesProperty].operator vector< ::DBus::Path>();
    EXPECT_EQ(2, devices.size());
  }
}

TEST_F(ManagerTest, GetServicesProperty) {
  AddMockProfileToManager(manager());
  map<string, ::DBus::Variant> props;
  ::DBus::Error dbus_error;
  DBusAdaptor::GetProperties(manager()->store(), &props, &dbus_error);
  map<string, ::DBus::Variant>::const_iterator prop =
      props.find(flimflam::kServicesProperty);
  ASSERT_FALSE(prop == props.end());
  const ::DBus::Variant &variant = prop->second;
  ASSERT_TRUE(DBusAdaptor::IsPaths(variant.signature()));
}

TEST_F(ManagerTest, MoveService) {
  Manager manager(control_interface(),
                  dispatcher(),
                  metrics(),
                  glib(),
                  run_path(),
                  storage_path(),
                  string());
  scoped_refptr<MockService> s2(new MockService(control_interface(),
                                                dispatcher(),
                                                metrics(),
                                                &manager));
  // Inject an actual profile, backed by a fake StoreInterface
  {
    Profile::Identifier id("irrelevant");
    ProfileRefPtr profile(
        new Profile(control_interface(), &manager, id, "", false));
    MockStore *storage = new MockStore;
    EXPECT_CALL(*storage, ContainsGroup(s2->GetStorageIdentifier()))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*storage, Flush())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    profile->set_storage(storage);
    AdoptProfile(&manager, profile);
  }
  // Create a profile that already has |s2| in it.
  ProfileRefPtr profile(new EphemeralProfile(control_interface(), &manager));
  EXPECT_TRUE(profile->AdoptService(s2));

  // Now, move the Service |s2| to another profile.
  EXPECT_CALL(*s2.get(), Save(_)).WillOnce(Return(true));
  ASSERT_TRUE(manager.MoveServiceToProfile(s2, manager.ActiveProfile()));

  // Force destruction of the original Profile, to ensure that the Service
  // is kept alive and populated with data.
  profile = NULL;
  ASSERT_TRUE(manager.ActiveProfile()->ContainsService(s2));
  manager.Stop();
}

TEST_F(ManagerTest, LookupProfileByRpcIdentifier) {
  scoped_refptr<MockProfile> mock_profile(
      new MockProfile(control_interface(), manager(), ""));
  const string kProfileName("profile0");
  EXPECT_CALL(*mock_profile, GetRpcIdentifier())
      .WillRepeatedly(Return(kProfileName));
  AdoptProfile(manager(), mock_profile);

  EXPECT_FALSE(manager()->LookupProfileByRpcIdentifier("foo"));
  ProfileRefPtr profile = manager()->LookupProfileByRpcIdentifier(kProfileName);
  EXPECT_EQ(mock_profile.get(), profile.get());
}

TEST_F(ManagerTest, SetProfileForService) {
  scoped_refptr<MockProfile> profile0(
      new MockProfile(control_interface(), manager(), ""));
  string profile_name0("profile0");
  EXPECT_CALL(*profile0, GetRpcIdentifier())
      .WillRepeatedly(Return(profile_name0));
  AdoptProfile(manager(), profile0);
  scoped_refptr<MockService> service(new MockService(control_interface(),
                                                     dispatcher(),
                                                     metrics(),
                                                     manager()));
  EXPECT_FALSE(manager()->HasService(service));
  {
    Error error;
    EXPECT_CALL(*profile0, AdoptService(_))
        .WillOnce(Return(true));
    // Expect that setting the profile of a service that does not already
    // have one assigned does not cause a crash.
    manager()->SetProfileForService(service, "profile0", &error);
    EXPECT_TRUE(error.IsSuccess());
  }

  // The service should be registered as a side-effect of the profile being
  // set for this service.
  EXPECT_TRUE(manager()->HasService(service));

  // Since we have mocked Profile::AdoptServie() above, the service's
  // profile was not actually changed.  Do so explicitly now.
  service->set_profile(profile0);

  {
    Error error;
    manager()->SetProfileForService(service, "foo", &error);
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ("Unknown Profile foo requested for Service", error.message());
  }

  {
    Error error;
    manager()->SetProfileForService(service, profile_name0, &error);
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ("Service is already connected to this profile", error.message());
  }

  scoped_refptr<MockProfile> profile1(
      new MockProfile(control_interface(), manager(), ""));
  string profile_name1("profile1");
  EXPECT_CALL(*profile1, GetRpcIdentifier())
      .WillRepeatedly(Return(profile_name1));
  AdoptProfile(manager(), profile1);

  {
    Error error;
    EXPECT_CALL(*profile1, AdoptService(_))
        .WillOnce(Return(true));
    EXPECT_CALL(*profile0, AbandonService(_))
        .WillOnce(Return(true));
    manager()->SetProfileForService(service, profile_name1, &error);
    EXPECT_TRUE(error.IsSuccess());
  }
}

TEST_F(ManagerTest, CreateProfile) {
  // It's much easier to use real Glib here since we want the storage
  // side-effects.
  GLib glib;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  Manager manager(control_interface(),
                  dispatcher(),
                  metrics(),
                  &glib,
                  run_path(),
                  storage_path(),
                  temp_dir.path().value());

  // Invalid name should be rejected.
  EXPECT_EQ(Error::kInvalidArguments, TestCreateProfile(&manager, ""));

  // A profile with invalid characters in it should similarly be rejected.
  EXPECT_EQ(Error::kInvalidArguments,
            TestCreateProfile(&manager, "valid_profile"));

  // We should be able to create a machine profile.
  EXPECT_EQ(Error::kSuccess, TestCreateProfile(&manager, "valid"));

  // We should succeed in creating a valid user profile.  Verify the returned
  // path.
  const char kProfile[] = "~user/profile";
  {
    Error error;
    string path;
    manager.CreateProfile(kProfile, &path, &error);
    EXPECT_EQ(Error::kSuccess, error.type());
    EXPECT_EQ("/profile_rpc", path);
  }

  // We should fail in creating it a second time (already exists).
  EXPECT_EQ(Error::kAlreadyExists, TestCreateProfile(&manager, kProfile));
}

TEST_F(ManagerTest, PushPopProfile) {
  // It's much easier to use real Glib in creating a Manager for this
  // test here since we want the storage side-effects.
  GLib glib;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  Manager manager(control_interface(),
                  dispatcher(),
                  metrics(),
                  &glib,
                  run_path(),
                  storage_path(),
                  temp_dir.path().value());

  // Pushing an invalid profile should fail.
  EXPECT_EQ(Error::kInvalidArguments, TestPushProfile(&manager, ""));

  // Pushing a default profile that does not exist should fail.
  EXPECT_EQ(Error::kNotFound, TestPushProfile(&manager, "default"));

  const char kProfile0[] = "~user/profile0";
  const char kProfile1[] = "~user/profile1";

  // Create a couple of profiles.
  ASSERT_EQ(Error::kSuccess, TestCreateProfile(&manager, kProfile0));
  ASSERT_EQ(Error::kSuccess, TestCreateProfile(&manager, kProfile1));

  // Push these profiles on the stack.
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kProfile0));
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kProfile1));

  // Pushing a profile a second time should fail.
  EXPECT_EQ(Error::kAlreadyExists, TestPushProfile(&manager, kProfile0));
  EXPECT_EQ(Error::kAlreadyExists, TestPushProfile(&manager, kProfile1));

  Error error;
  // Active profile should be the last one we pushed.
  EXPECT_EQ(kProfile1, "~" + manager.ActiveProfile()->GetFriendlyName());

  // Make sure a profile name that doesn't exist fails.
  const char kProfile2Id[] = "profile2";
  const string kProfile2 = base::StringPrintf("~user/%s", kProfile2Id);
  EXPECT_EQ(Error::kNotFound, TestPushProfile(&manager, kProfile2));

  // Create a new service, with a specific storage name.
  scoped_refptr<MockService> service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                &manager));
  const char kServiceName[] = "service_storage_name";
  EXPECT_CALL(*service.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(kServiceName));
  EXPECT_CALL(*service.get(), Load(_))
      .WillRepeatedly(Return(true));

  // Add this service to the manager -- it should end up in the ephemeral
  // profile.
  manager.RegisterService(service);
  ASSERT_EQ(GetEphemeralProfile(&manager), service->profile());

  // Create storage for a profile that contains the service storage name.
  ASSERT_TRUE(CreateBackingStoreForService(&temp_dir, kProfile2Id,
                                           kServiceName));

  // When we push the profile, the service should move away from the
  // ephemeral profile to this new profile since it has an entry for
  // this service.
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kProfile2));
  EXPECT_NE(GetEphemeralProfile(&manager), service->profile());
  EXPECT_EQ(kProfile2, "~" + service->profile()->GetFriendlyName());

  // Insert another profile that should supersede ownership of the service.
  const char kProfile3Id[] = "profile3";
  const string kProfile3 = base::StringPrintf("~user/%s", kProfile3Id);
  ASSERT_TRUE(CreateBackingStoreForService(&temp_dir, kProfile3Id,
                                           kServiceName));
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kProfile3));
  EXPECT_EQ(kProfile3, "~" + service->profile()->GetFriendlyName());

  // Popping an invalid profile name should fail.
  EXPECT_EQ(Error::kInvalidArguments, TestPopProfile(&manager, "~"));

  // Popping an profile that is not at the top of the stack should fail.
  EXPECT_EQ(Error::kNotSupported, TestPopProfile(&manager, kProfile0));

  // Popping the top profile should succeed.
  EXPECT_EQ(Error::kSuccess, TestPopProfile(&manager, kProfile3));

  // Moreover the service should have switched profiles to profile 2.
  EXPECT_EQ(kProfile2, "~" + service->profile()->GetFriendlyName());

  // Popping the top profile should succeed.
  EXPECT_EQ(Error::kSuccess, TestPopAnyProfile(&manager));

  // The service should now revert to the ephemeral profile.
  EXPECT_EQ(GetEphemeralProfile(&manager), service->profile());

  // Pop the remaining two services off the stack.
  EXPECT_EQ(Error::kSuccess, TestPopAnyProfile(&manager));
  EXPECT_EQ(Error::kSuccess, TestPopAnyProfile(&manager));

  // Next pop should fail with "stack is empty".
  EXPECT_EQ(Error::kNotFound, TestPopAnyProfile(&manager));

  const char kMachineProfile0[] = "machineprofile0";
  const char kMachineProfile1[] = "machineprofile1";
  ASSERT_EQ(Error::kSuccess, TestCreateProfile(&manager, kMachineProfile0));
  ASSERT_EQ(Error::kSuccess, TestCreateProfile(&manager, kMachineProfile1));

  // Should be able to push a machine profile.
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kMachineProfile0));

  // Should be able to push a user profile atop a machine profile.
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kProfile0));

  // Pushing a system-wide profile on top of a user profile should fail.
  EXPECT_EQ(Error::kInvalidArguments,
            TestPushProfile(&manager, kMachineProfile1));

  // However if we pop the user profile, we should be able stack another
  // machine profile on.
  EXPECT_EQ(Error::kSuccess, TestPopAnyProfile(&manager));
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kMachineProfile1));
}

TEST_F(ManagerTest, RemoveProfile) {
  // It's much easier to use real Glib in creating a Manager for this
  // test here since we want the storage side-effects.
  GLib glib;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  Manager manager(control_interface(),
                  dispatcher(),
                  metrics(),
                  &glib,
                  run_path(),
                  storage_path(),
                  temp_dir.path().value());

  const char kProfile0[] = "profile0";
  FilePath profile_path(
      FilePath(storage_path()).Append(string(kProfile0) + ".profile"));

  ASSERT_EQ(Error::kSuccess, TestCreateProfile(&manager, kProfile0));
  ASSERT_TRUE(file_util::PathExists(profile_path));

  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kProfile0));

  // Remove should fail since the profile is still on the stack.
  {
    Error error;
    manager.RemoveProfile(kProfile0, &error);
    EXPECT_EQ(Error::kInvalidArguments, error.type());
  }

  // Profile path should still exist.
  EXPECT_TRUE(file_util::PathExists(profile_path));

  EXPECT_EQ(Error::kSuccess, TestPopAnyProfile(&manager));

  // This should succeed now that the profile is off the stack.
  {
    Error error;
    manager.RemoveProfile(kProfile0, &error);
    EXPECT_EQ(Error::kSuccess, error.type());
  }

  // Profile path should no longer exist.
  EXPECT_FALSE(file_util::PathExists(profile_path));

  // Another remove succeeds, due to a foible in file_util::Delete --
  // it is not an error to delete a file that does not exist.
  {
    Error error;
    manager.RemoveProfile(kProfile0, &error);
    EXPECT_EQ(Error::kSuccess, error.type());
  }

  // Let's create an error case that will "work".  Create a non-empty
  // directory in the place of the profile pathname.
  ASSERT_TRUE(file_util::CreateDirectory(profile_path.Append("foo")));
  {
    Error error;
    manager.RemoveProfile(kProfile0, &error);
    EXPECT_EQ(Error::kOperationFailed, error.type());
  }
}

TEST_F(ManagerTest, CreateDuplicateProfileWithMissingKeyfile) {
  // It's much easier to use real Glib in creating a Manager for this
  // test here since we want the storage side-effects.
  GLib glib;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  Manager manager(control_interface(),
                  dispatcher(),
                  metrics(),
                  &glib,
                  run_path(),
                  storage_path(),
                  temp_dir.path().value());

  const char kProfile0[] = "profile0";
  FilePath profile_path(
      FilePath(storage_path()).Append(string(kProfile0) + ".profile"));

  ASSERT_EQ(Error::kSuccess, TestCreateProfile(&manager, kProfile0));
  ASSERT_TRUE(file_util::PathExists(profile_path));
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kProfile0));

  // Ensure that even if the backing filestore is removed, we still can't
  // create a profile twice.
  ASSERT_TRUE(file_util::Delete(profile_path, false));
  EXPECT_EQ(Error::kAlreadyExists, TestCreateProfile(&manager, kProfile0));
}

// Use this matcher instead of passing RefPtrs directly into the arguments
// of EXPECT_CALL() because otherwise we may create un-cleaned-up references at
// system teardown.
MATCHER_P(IsRefPtrTo, ref_address, "") {
  return arg.get() == ref_address;
}

TEST_F(ManagerTest, HandleProfileEntryDeletion) {
  MockServiceRefPtr s_not_in_profile(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_not_in_group(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_configure_fail(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_configure_succeed(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  string entry_name("entry_name");
  EXPECT_CALL(*s_not_in_profile.get(), GetStorageIdentifier()).Times(0);
  EXPECT_CALL(*s_not_in_group.get(), GetStorageIdentifier())
      .WillRepeatedly(Return("not_entry_name"));
  EXPECT_CALL(*s_configure_fail.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(entry_name));
  EXPECT_CALL(*s_configure_succeed.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(entry_name));

  manager()->RegisterService(s_not_in_profile);
  manager()->RegisterService(s_not_in_group);
  manager()->RegisterService(s_configure_fail);
  manager()->RegisterService(s_configure_succeed);

  scoped_refptr<MockProfile> profile0(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));
  scoped_refptr<MockProfile> profile1(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));

  s_not_in_group->set_profile(profile1);
  s_configure_fail->set_profile(profile1);
  s_configure_succeed->set_profile(profile1);

  AdoptProfile(manager(), profile0);
  AdoptProfile(manager(), profile1);

  // No services are a member of this profile.
  EXPECT_FALSE(manager()->HandleProfileEntryDeletion(profile0, entry_name));

  // No services that are members of this profile have this entry name.
  EXPECT_FALSE(manager()->HandleProfileEntryDeletion(profile1, ""));

  // Only services that are members of the profile and group will be abandoned.
  EXPECT_CALL(*profile1.get(),
              AbandonService(IsRefPtrTo(s_not_in_profile.get()))).Times(0);
  EXPECT_CALL(*profile1.get(),
              AbandonService(IsRefPtrTo(s_not_in_group.get()))).Times(0);
  EXPECT_CALL(*profile1.get(),
              AbandonService(IsRefPtrTo(s_configure_fail.get())))
      .WillOnce(Return(true));
  EXPECT_CALL(*profile1.get(),
              AbandonService(IsRefPtrTo(s_configure_succeed.get())))
      .WillOnce(Return(true));

  // Never allow services to re-join profile1.
  EXPECT_CALL(*profile1.get(), ConfigureService(_))
      .WillRepeatedly(Return(false));

  // Only allow one of the members of the profile and group to successfully
  // join profile0.
  EXPECT_CALL(*profile0.get(),
              ConfigureService(IsRefPtrTo(s_not_in_profile.get()))).Times(0);
  EXPECT_CALL(*profile0.get(),
              ConfigureService(IsRefPtrTo(s_not_in_group.get()))).Times(0);
  EXPECT_CALL(*profile0.get(),
              ConfigureService(IsRefPtrTo(s_configure_fail.get())))
      .WillOnce(Return(false));
  EXPECT_CALL(*profile0.get(),
              ConfigureService(IsRefPtrTo(s_configure_succeed.get())))
      .WillOnce(Return(true));

  // Expect the failed-to-configure service to have Unload() called on it.
  EXPECT_CALL(*s_not_in_profile.get(), Unload()).Times(0);
  EXPECT_CALL(*s_not_in_group.get(), Unload()).Times(0);
  EXPECT_CALL(*s_configure_fail.get(), Unload()).Times(1);
  EXPECT_CALL(*s_configure_succeed.get(), Unload()).Times(0);

  EXPECT_TRUE(manager()->HandleProfileEntryDeletion(profile1, entry_name));

  EXPECT_EQ(GetEphemeralProfile(manager()), s_not_in_profile->profile().get());
  EXPECT_EQ(profile1, s_not_in_group->profile());
  EXPECT_EQ(GetEphemeralProfile(manager()), s_configure_fail->profile());

  // Since we are using a MockProfile, the profile does not actually change,
  // since ConfigureService was not actually called on the service.
  EXPECT_EQ(profile1, s_configure_succeed->profile());
}

TEST_F(ManagerTest, HandleProfileEntryDeletionWithUnload) {
  MockServiceRefPtr s_will_remove0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_will_remove1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_will_not_remove0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_will_not_remove1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  EXPECT_CALL(*metrics(), NotifyDefaultServiceChanged(NULL))
      .Times(4);  // Once for each registration.

  string entry_name("entry_name");
  EXPECT_CALL(*s_will_remove0.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(entry_name));
  EXPECT_CALL(*s_will_remove1.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(entry_name));
  EXPECT_CALL(*s_will_not_remove0.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(entry_name));
  EXPECT_CALL(*s_will_not_remove1.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(entry_name));

  manager()->RegisterService(s_will_remove0);
  CompleteServiceSort();
  manager()->RegisterService(s_will_not_remove0);
  CompleteServiceSort();
  manager()->RegisterService(s_will_remove1);
  CompleteServiceSort();
  manager()->RegisterService(s_will_not_remove1);
  CompleteServiceSort();

  // One for each service added above.
  ASSERT_EQ(4, manager()->services_.size());

  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));

  s_will_remove0->set_profile(profile);
  s_will_remove1->set_profile(profile);
  s_will_not_remove0->set_profile(profile);
  s_will_not_remove1->set_profile(profile);

  AdoptProfile(manager(), profile);

  // Deny any of the services re-entry to the profile.
  EXPECT_CALL(*profile, ConfigureService(_))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*profile, AbandonService(ServiceRefPtr(s_will_remove0)))
      .WillOnce(Return(true));
  EXPECT_CALL(*profile, AbandonService(ServiceRefPtr(s_will_remove1)))
      .WillOnce(Return(true));
  EXPECT_CALL(*profile, AbandonService(ServiceRefPtr(s_will_not_remove0)))
      .WillOnce(Return(true));
  EXPECT_CALL(*profile, AbandonService(ServiceRefPtr(s_will_not_remove1)))
      .WillOnce(Return(true));

  EXPECT_CALL(*s_will_remove0, Unload())
      .WillOnce(Return(true));
  EXPECT_CALL(*s_will_remove1, Unload())
      .WillOnce(Return(true));
  EXPECT_CALL(*s_will_not_remove0, Unload())
      .WillOnce(Return(false));
  EXPECT_CALL(*s_will_not_remove1, Unload())
      .WillOnce(Return(false));


  // This will cause all the profiles to be unloaded.
  EXPECT_TRUE(manager()->HandleProfileEntryDeletion(profile, entry_name));

  // 2 of the 4 services added above should have been unregistered and
  // removed, leaving 2.
  EXPECT_EQ(2, manager()->services_.size());
  EXPECT_EQ(s_will_not_remove0.get(), manager()->services_[0].get());
  EXPECT_EQ(s_will_not_remove1.get(), manager()->services_[1].get());
}

TEST_F(ManagerTest, PopProfileWithUnload) {
  MockServiceRefPtr s_will_remove0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_will_remove1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_will_not_remove0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  MockServiceRefPtr s_will_not_remove1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  EXPECT_CALL(*metrics(), NotifyDefaultServiceChanged(NULL))
      .Times(5);  // Once for each registration, and one after profile pop.

  manager()->RegisterService(s_will_remove0);
  CompleteServiceSort();
  manager()->RegisterService(s_will_not_remove0);
  CompleteServiceSort();
  manager()->RegisterService(s_will_remove1);
  CompleteServiceSort();
  manager()->RegisterService(s_will_not_remove1);
  CompleteServiceSort();

  // One for each service added above.
  ASSERT_EQ(4, manager()->services_.size());

  scoped_refptr<MockProfile> profile0(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));
  scoped_refptr<MockProfile> profile1(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));

  s_will_remove0->set_profile(profile1);
  s_will_remove1->set_profile(profile1);
  s_will_not_remove0->set_profile(profile1);
  s_will_not_remove1->set_profile(profile1);

  AdoptProfile(manager(), profile0);
  AdoptProfile(manager(), profile1);

  // Deny any of the services entry to profile0, so they will all be unloaded.
  EXPECT_CALL(*profile0, ConfigureService(_))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*s_will_remove0, Unload())
      .WillOnce(Return(true));
  EXPECT_CALL(*s_will_remove1, Unload())
      .WillOnce(Return(true));
  EXPECT_CALL(*s_will_not_remove0, Unload())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*s_will_not_remove1, Unload())
      .WillOnce(Return(false));

  // This will pop profile1, which should cause all our profiles to unload.
  manager()->PopProfileInternal();
  CompleteServiceSort();

  // 2 of the 4 services added above should have been unregistered and
  // removed, leaving 2.
  EXPECT_EQ(2, manager()->services_.size());
  EXPECT_EQ(s_will_not_remove0.get(), manager()->services_[0].get());
  EXPECT_EQ(s_will_not_remove1.get(), manager()->services_[1].get());

  // Expect the unloaded services to lose their profile reference.
  EXPECT_FALSE(s_will_remove0->profile());
  EXPECT_FALSE(s_will_remove1->profile());

  // If we explicitly deregister a service, the effect should be the same
  // with respect to the profile reference.
  ASSERT_TRUE(s_will_not_remove0->profile());
  manager()->DeregisterService(s_will_not_remove0);
  EXPECT_FALSE(s_will_not_remove0->profile());
}

TEST_F(ManagerTest, SetProperty) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(manager()->mutable_store(),
                                         flimflam::kOfflineModeProperty,
                                         PropertyStoreTest::kBoolV,
                                         &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(manager()->mutable_store(),
                                         flimflam::kCountryProperty,
                                         PropertyStoreTest::kStringV,
                                         &error));
  }
  // Attempt to write with value of wrong type should return InvalidArgs.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(manager()->mutable_store(),
                                          flimflam::kCountryProperty,
                                          PropertyStoreTest::kBoolV,
                                          &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(manager()->mutable_store(),
                                          flimflam::kOfflineModeProperty,
                                          PropertyStoreTest::kStringV,
                                          &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  // Attempt to write R/O property should return InvalidArgs.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(
        manager()->mutable_store(),
        flimflam::kEnabledTechnologiesProperty,
        PropertyStoreTest::kStringsV,
        &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
}

TEST_F(ManagerTest, RequestScan) {
  {
    Error error;
    manager()->RegisterDevice(mock_devices_[0].get());
    manager()->RegisterDevice(mock_devices_[1].get());
    EXPECT_CALL(*mock_devices_[0], technology())
        .WillRepeatedly(Return(Technology::kWifi));
    EXPECT_CALL(*mock_devices_[0], Scan(_));
    EXPECT_CALL(*mock_devices_[1], technology())
        .WillRepeatedly(Return(Technology::kUnknown));
    EXPECT_CALL(*mock_devices_[1], Scan(_)).Times(0);
    manager()->RequestScan(flimflam::kTypeWifi, &error);
  }

  {
    Error error;
    manager()->RequestScan("bogus_device_type", &error);
    EXPECT_EQ(Error::kInvalidArguments, error.type());
  }
}

TEST_F(ManagerTest, GetServiceNoType) {
  KeyValueStore args;
  Error e;
  manager()->GetService(args, &e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("must specify service type", e.message());
}

TEST_F(ManagerTest, GetServiceUnknownType) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeEthernet);
  manager()->GetService(args, &e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_EQ("service type is unsupported", e.message());
}

TEST_F(ManagerTest, GetServiceNoWifiDevice) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWifi);
  manager()->GetService(args, &e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("no wifi devices available", e.message());
}

TEST_F(ManagerTest, GetServiceWifi) {
  KeyValueStore args;
  Error e;
  WiFiServiceRefPtr wifi_service;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWifi);
  manager()->RegisterDevice(mock_wifi_);
  EXPECT_CALL(*mock_wifi_, GetService(_, _))
      .WillRepeatedly(Return(wifi_service));
  manager()->GetService(args, &e);
  EXPECT_TRUE(e.IsSuccess());
}

TEST_F(ManagerTest, GetServiceVPNUnknownType) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeVPN);
  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));
  AdoptProfile(manager(), profile);
  ServiceRefPtr service = manager()->GetService(args, &e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_FALSE(service);
}

TEST_F(ManagerTest, GetServiceVPN) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeVPN);
  args.SetString(flimflam::kProviderTypeProperty, flimflam::kProviderOpenVpn);
  args.SetString(flimflam::kProviderHostProperty, "10.8.0.1");
  args.SetString(flimflam::kProviderNameProperty, "vpn-name");
  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));
  AdoptProfile(manager(), profile);
  ServiceRefPtr updated_service;
  EXPECT_CALL(*profile, UpdateService(_))
      .WillOnce(DoAll(SaveArg<0>(&updated_service), Return(true)));
  ServiceRefPtr configured_service;
  EXPECT_CALL(*profile, ConfigureService(_))
      .WillOnce(DoAll(SaveArg<0>(&configured_service), Return(true)));
  ServiceRefPtr service = manager()->GetService(args, &e);
  EXPECT_TRUE(e.IsSuccess());
  EXPECT_TRUE(service);
  EXPECT_EQ(service, updated_service);
  EXPECT_EQ(service, configured_service);
}

TEST_F(ManagerTest, GetServiceWiMaxNoNetworkId) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWimax);
  ServiceRefPtr service = manager()->GetService(args, &e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("Missing WiMAX network id.", e.message());
  EXPECT_FALSE(service);
}

TEST_F(ManagerTest, GetServiceWiMax) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWimax);
  args.SetString(WiMaxService::kNetworkIdProperty, "01234567");
  args.SetString(flimflam::kNameProperty, "WiMAX Network");
  ServiceRefPtr service = manager()->GetService(args, &e);
  EXPECT_TRUE(e.IsSuccess());
  EXPECT_TRUE(service);
}

TEST_F(ManagerTest, ConfigureServiceWithInvalidProfile) {
  // Manager calls ActiveProfile() so we need at least one profile installed.
  scoped_refptr<MockProfile> profile(
      new NiceMock<MockProfile>(control_interface(), manager(), ""));
  AdoptProfile(manager(), profile);

  KeyValueStore args;
  args.SetString(flimflam::kProfileProperty, "xxx");
  Error error;
  manager()->ConfigureService(args, &error);
  EXPECT_EQ(Error::kInvalidArguments, error.type());
  EXPECT_EQ("Invalid profile name xxx", error.message());
}

TEST_F(ManagerTest, ConfigureServiceWithGetServiceFailure) {
  // Manager calls ActiveProfile() so we need at least one profile installed.
  scoped_refptr<MockProfile> profile(
      new NiceMock<MockProfile>(control_interface(), manager(), ""));
  AdoptProfile(manager(), profile);

  KeyValueStore args;
  Error error;
  manager()->ConfigureService(args, &error);
  EXPECT_EQ(Error::kInvalidArguments, error.type());
  EXPECT_EQ("must specify service type", error.message());
}

// A registered service in the ephemeral profile should be moved to the
// active profile as a part of configuration if no profile was explicitly
// specified.
TEST_F(ManagerTest, ConfigureRegisteredServiceWithoutProfile) {
  scoped_refptr<MockProfile> profile(
      new NiceMock<MockProfile>(control_interface(), manager(), ""));

  AdoptProfile(manager(), profile);  // This is now the active profile.

  const std::vector<uint8_t> ssid;
  scoped_refptr<MockWiFiService> service(
      new NiceMock<MockWiFiService>(control_interface(),
                                    dispatcher(),
                                    metrics(),
                                    manager(),
                                    mock_wifi_,
                                    ssid,
                                    "",
                                    "",
                                    false));

  manager()->RegisterService(service);
  service->set_profile(GetEphemeralProfile(manager()));

  // A separate MockWiFi from mock_wifi_ is used in the Manager since using
  // the same device as that used above causes a refcounting loop.
  scoped_refptr<MockWiFi> wifi(new NiceMock<MockWiFi>(control_interface(),
                                                      dispatcher(),
                                                      metrics(),
                                                      manager(),
                                                      "wifi1",
                                                      "addr5",
                                                      5));
  manager()->RegisterDevice(wifi);
  EXPECT_CALL(*wifi, GetService(_, _))
      .WillOnce(Return(service));
  EXPECT_CALL(*profile, UpdateService(ServiceRefPtr(service.get())))
      .WillOnce(Return(true));
  EXPECT_CALL(*profile, AdoptService(ServiceRefPtr(service.get())))
      .WillOnce(Return(true));

  KeyValueStore args;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWifi);
  Error error;
  manager()->ConfigureService(args, &error);
  EXPECT_TRUE(error.IsSuccess());
}

// If were configure a service that was already registered and explicitly
// specify a profile, it should be moved from the profile it was previously
// in to the specified profile if one was requested.
TEST_F(ManagerTest, ConfigureRegisteredServiceWithProfile) {
  scoped_refptr<MockProfile> profile0(
      new NiceMock<MockProfile>(control_interface(), manager(), ""));
  scoped_refptr<MockProfile> profile1(
      new NiceMock<MockProfile>(control_interface(), manager(), ""));

  const string kProfileName0 = "profile0";
  const string kProfileName1 = "profile1";

  EXPECT_CALL(*profile0, GetRpcIdentifier())
      .WillRepeatedly(Return(kProfileName0));
  EXPECT_CALL(*profile1, GetRpcIdentifier())
      .WillRepeatedly(Return(kProfileName1));

  AdoptProfile(manager(), profile0);
  AdoptProfile(manager(), profile1);  // profile1 is now the ActiveProfile.

  const std::vector<uint8_t> ssid;
  scoped_refptr<MockWiFiService> service(
      new NiceMock<MockWiFiService>(control_interface(),
                                    dispatcher(),
                                    metrics(),
                                    manager(),
                                    mock_wifi_,
                                    ssid,
                                    "",
                                    "",
                                    false));

  manager()->RegisterService(service);
  service->set_profile(profile1);

  // A separate MockWiFi from mock_wifi_ is used in the Manager since using
  // the same device as that used above causes a refcounting loop.
  scoped_refptr<MockWiFi> wifi(new NiceMock<MockWiFi>(control_interface(),
                                                      dispatcher(),
                                                      metrics(),
                                                      manager(),
                                                      "wifi1",
                                                      "addr5",
                                                      5));
  manager()->RegisterDevice(wifi);
  EXPECT_CALL(*wifi, GetService(_, _))
      .WillOnce(Return(service));
  EXPECT_CALL(*profile0, UpdateService(ServiceRefPtr(service.get())))
      .WillOnce(Return(true));
  EXPECT_CALL(*profile0, AdoptService(ServiceRefPtr(service.get())))
      .WillOnce(Return(true));
  EXPECT_CALL(*profile1, AbandonService(ServiceRefPtr(service.get())))
      .WillOnce(Return(true));

  KeyValueStore args;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWifi);
  args.SetString(flimflam::kProfileProperty, kProfileName0);
  Error error;
  manager()->ConfigureService(args, &error);
  EXPECT_TRUE(error.IsSuccess());
  service->set_profile(NULL);  // Breaks refcounting loop.
}

// An unregistered service should remain unregistered, but its contents should
// be saved to the specified profile nonetheless.
TEST_F(ManagerTest, ConfigureUnregisteredServiceWithProfile) {
  scoped_refptr<MockProfile> profile0(
      new NiceMock<MockProfile>(control_interface(), manager(), ""));
  scoped_refptr<MockProfile> profile1(
      new NiceMock<MockProfile>(control_interface(), manager(), ""));

  const string kProfileName0 = "profile0";
  const string kProfileName1 = "profile1";

  EXPECT_CALL(*profile0, GetRpcIdentifier())
      .WillRepeatedly(Return(kProfileName0));
  EXPECT_CALL(*profile1, GetRpcIdentifier())
      .WillRepeatedly(Return(kProfileName1));

  AdoptProfile(manager(), profile0);
  AdoptProfile(manager(), profile1);  // profile1 is now the ActiveProfile.

  const std::vector<uint8_t> ssid;
  scoped_refptr<MockWiFiService> service(
      new NiceMock<MockWiFiService>(control_interface(),
                                    dispatcher(),
                                    metrics(),
                                    manager(),
                                    mock_wifi_,
                                    ssid,
                                    "",
                                    "",
                                    false));

  service->set_profile(profile1);

  // A separate MockWiFi from mock_wifi_ is used in the Manager since using
  // the same device as that used above causes a refcounting loop.
  scoped_refptr<MockWiFi> wifi(new NiceMock<MockWiFi>(control_interface(),
                                                      dispatcher(),
                                                      metrics(),
                                                      manager(),
                                                      "wifi1",
                                                      "addr5",
                                                      5));
  manager()->RegisterDevice(wifi);
  EXPECT_CALL(*wifi, GetService(_, _))
      .WillOnce(Return(service));
  EXPECT_CALL(*profile0, UpdateService(ServiceRefPtr(service.get())))
      .WillOnce(Return(true));
  EXPECT_CALL(*profile0, AdoptService(_))
      .Times(0);
  EXPECT_CALL(*profile1, AdoptService(_))
      .Times(0);

  KeyValueStore args;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWifi);
  args.SetString(flimflam::kProfileProperty, kProfileName0);
  Error error;
  manager()->ConfigureService(args, &error);
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(ManagerTest, TechnologyOrder) {
  Error error;
  manager()->SetTechnologyOrder(string(flimflam::kTypeEthernet) + "," +
                                string(flimflam::kTypeWifi), &error);
  ASSERT_TRUE(error.IsSuccess());
  EXPECT_EQ(manager()->GetTechnologyOrder(),
            string(flimflam::kTypeEthernet) + "," +
            string(flimflam::kTypeWifi));

  manager()->SetTechnologyOrder(string(flimflam::kTypeEthernet) + "x," +
                                string(flimflam::kTypeWifi), &error);
  ASSERT_FALSE(error.IsSuccess());
  EXPECT_EQ(Error::kInvalidArguments, error.type());
  EXPECT_EQ(string(flimflam::kTypeEthernet) + "," +
            string(flimflam::kTypeWifi),
            manager()->GetTechnologyOrder());
}

TEST_F(ManagerTest, SortServices) {
  // TODO(quiche): Some of these tests would probably fit better in
  // service_unittest, since the actual comparison of Services is
  // implemented in Service. (crosbug.com/23370)

  scoped_refptr<MockService> mock_service0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  scoped_refptr<MockService> mock_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  manager()->RegisterService(mock_service0);
  manager()->RegisterService(mock_service1);

  // Services should already be sorted by UniqueName
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Asking explictly to sort services should not change anything
  manager()->SortServicesTask();
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Two otherwise equal services should be reordered by strength
  mock_service1->SetStrength(1);
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  // Security
  mock_service0->set_security_level(1);
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Technology
  EXPECT_CALL(*mock_service0.get(), technology())
      .WillRepeatedly(Return((Technology::kWifi)));
  EXPECT_CALL(*mock_service1.get(), technology())
      .WillRepeatedly(Return(Technology::kEthernet));

  Error error;
  // Default technology ordering should favor Ethernet over WiFi.
  manager()->SortServicesTask();
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  manager()->SetTechnologyOrder(string(flimflam::kTypeWifi) + "," +
                                string(flimflam::kTypeEthernet), &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Priority.
  mock_service0->set_priority(1);
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Favorite.
  mock_service1->MakeFavorite();
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  // Auto-connect.
  mock_service0->set_auto_connect(true);
  manager()->UpdateService(mock_service0);
  mock_service1->set_auto_connect(false);
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Connectable.
  mock_service1->set_connectable(true);
  manager()->UpdateService(mock_service1);
  mock_service0->set_connectable(false);
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  // IsFailed.
  EXPECT_CALL(*mock_service0.get(), state())
      .WillRepeatedly(Return(Service::kStateIdle));
  EXPECT_CALL(*mock_service0.get(), IsFailed())
      .WillRepeatedly(Return(false));
  manager()->UpdateService(mock_service0);
  EXPECT_CALL(*mock_service0.get(), state())
      .WillRepeatedly(Return(Service::kStateFailure));
  EXPECT_CALL(*mock_service1.get(), IsFailed())
      .WillRepeatedly(Return(true));
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Connecting.
  EXPECT_CALL(*mock_service1.get(), state())
      .WillRepeatedly(Return(Service::kStateAssociating));
  EXPECT_CALL(*mock_service1.get(), IsConnecting())
      .WillRepeatedly(Return(true));
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  // Connected.
  EXPECT_CALL(*mock_service0.get(), state())
      .WillRepeatedly(Return(Service::kStatePortal));
  EXPECT_CALL(*mock_service0.get(), IsConnected())
      .WillRepeatedly(Return(true));
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Portal.
  EXPECT_CALL(*mock_service1.get(), state())
      .WillRepeatedly(Return(Service::kStateConnected));
  EXPECT_CALL(*mock_service1.get(), IsConnected())
      .WillRepeatedly(Return(true));
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  manager()->DeregisterService(mock_service0);
  manager()->DeregisterService(mock_service1);
}

TEST_F(ManagerTest, SortServicesWithConnection) {
  MockMetrics mock_metrics;
  manager()->set_metrics(&mock_metrics);

  scoped_refptr<MockService> mock_service0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  scoped_refptr<MockService> mock_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  scoped_refptr<MockConnection> mock_connection0(
      new NiceMock<MockConnection>(device_info_.get()));
  scoped_refptr<MockConnection> mock_connection1(
      new NiceMock<MockConnection>(device_info_.get()));

  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(NULL));
  manager()->RegisterService(mock_service0);
  CompleteServiceSort();
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(NULL));
  manager()->RegisterService(mock_service1);
  CompleteServiceSort();

  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(NULL));
  manager()->SortServicesTask();

  mock_service1->set_priority(1);
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(NULL));
  manager()->SortServicesTask();

  mock_service1->set_priority(0);
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(NULL));
  manager()->SortServicesTask();

  mock_service0->set_mock_connection(mock_connection0);
  mock_service1->set_mock_connection(mock_connection1);

  EXPECT_CALL(*mock_connection0.get(), SetIsDefault(true));
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(mock_service0.get()));
  manager()->SortServicesTask();

  mock_service1->set_priority(1);
  EXPECT_CALL(*mock_connection0.get(), SetIsDefault(false));
  EXPECT_CALL(*mock_connection1.get(), SetIsDefault(true));
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(mock_service1.get()));
  manager()->SortServicesTask();

  EXPECT_CALL(*mock_connection0.get(), SetIsDefault(true));
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(mock_service0.get()));
  mock_service1->set_mock_connection(NULL);
  manager()->DeregisterService(mock_service1);
  CompleteServiceSort();

  mock_service0->set_mock_connection(NULL);
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(NULL));
  manager()->DeregisterService(mock_service0);
  CompleteServiceSort();

  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(NULL));
  manager()->SortServicesTask();
}

TEST_F(ManagerTest, AvailableTechnologies) {
  mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                   dispatcher(),
                                                   metrics(),
                                                   manager(),
                                                   "null4",
                                                   "addr4",
                                                   0));
  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  manager()->RegisterDevice(mock_devices_[2]);
  manager()->RegisterDevice(mock_devices_[3]);

  ON_CALL(*mock_devices_[0].get(), technology())
      .WillByDefault(Return(Technology::kEthernet));
  ON_CALL(*mock_devices_[1].get(), technology())
      .WillByDefault(Return(Technology::kWifi));
  ON_CALL(*mock_devices_[2].get(), technology())
      .WillByDefault(Return(Technology::kCellular));
  ON_CALL(*mock_devices_[3].get(), technology())
      .WillByDefault(Return(Technology::kWifi));

  set<string> expected_technologies;
  expected_technologies.insert(Technology::NameFromIdentifier(
      Technology::kEthernet));
  expected_technologies.insert(Technology::NameFromIdentifier(
      Technology::kWifi));
  expected_technologies.insert(Technology::NameFromIdentifier(
      Technology::kCellular));
  Error error;
  vector<string> technologies = manager()->AvailableTechnologies(&error);

  EXPECT_THAT(set<string>(technologies.begin(), technologies.end()),
              ContainerEq(expected_technologies));
}

TEST_F(ManagerTest, ConnectedTechnologies) {
  scoped_refptr<MockService> connected_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  scoped_refptr<MockService> connected_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  scoped_refptr<MockService> disconnected_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  scoped_refptr<MockService> disconnected_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  ON_CALL(*connected_service1.get(), IsConnected())
      .WillByDefault(Return(true));
  ON_CALL(*connected_service2.get(), IsConnected())
      .WillByDefault(Return(true));

  manager()->RegisterService(connected_service1);
  manager()->RegisterService(connected_service2);
  manager()->RegisterService(disconnected_service1);
  manager()->RegisterService(disconnected_service2);

  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  manager()->RegisterDevice(mock_devices_[2]);
  manager()->RegisterDevice(mock_devices_[3]);

  ON_CALL(*mock_devices_[0].get(), technology())
      .WillByDefault(Return(Technology::kEthernet));
  ON_CALL(*mock_devices_[1].get(), technology())
      .WillByDefault(Return(Technology::kWifi));
  ON_CALL(*mock_devices_[2].get(), technology())
      .WillByDefault(Return(Technology::kCellular));
  ON_CALL(*mock_devices_[3].get(), technology())
      .WillByDefault(Return(Technology::kWifi));

  mock_devices_[0]->SelectService(connected_service1);
  mock_devices_[1]->SelectService(disconnected_service1);
  mock_devices_[2]->SelectService(disconnected_service2);
  mock_devices_[3]->SelectService(connected_service2);

  set<string> expected_technologies;
  expected_technologies.insert(Technology::NameFromIdentifier(
      Technology::kEthernet));
  expected_technologies.insert(Technology::NameFromIdentifier(
      Technology::kWifi));
  Error error;

  vector<string> technologies = manager()->ConnectedTechnologies(&error);
  EXPECT_THAT(set<string>(technologies.begin(), technologies.end()),
              ContainerEq(expected_technologies));
}

TEST_F(ManagerTest, DefaultTechnology) {
  scoped_refptr<MockService> connected_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  scoped_refptr<MockService> disconnected_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  // Connected. WiFi.
  ON_CALL(*connected_service.get(), IsConnected())
      .WillByDefault(Return(true));
  ON_CALL(*connected_service.get(), state())
      .WillByDefault(Return(Service::kStateConnected));
  ON_CALL(*connected_service.get(), technology())
      .WillByDefault(Return(Technology::kWifi));

  // Disconnected. Ethernet.
  ON_CALL(*disconnected_service.get(), technology())
      .WillByDefault(Return(Technology::kEthernet));

  manager()->RegisterService(disconnected_service);
  CompleteServiceSort();
  Error error;
  EXPECT_THAT(manager()->DefaultTechnology(&error), StrEq(""));


  manager()->RegisterService(connected_service);
  CompleteServiceSort();
  // Connected service should be brought to the front now.
  string expected_technology =
      Technology::NameFromIdentifier(Technology::kWifi);
  EXPECT_THAT(manager()->DefaultTechnology(&error), StrEq(expected_technology));
}

TEST_F(ManagerTest, DisconnectServicesOnStop) {
  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  manager()->RegisterService(mock_service);
  EXPECT_CALL(*mock_service.get(), Disconnect(_)).Times(1);
  manager()->Stop();
}

TEST_F(ManagerTest, UpdateServiceConnected) {
  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  manager()->RegisterService(mock_service);
  EXPECT_FALSE(mock_service->favorite());
  EXPECT_FALSE(mock_service->auto_connect());

  EXPECT_CALL(*mock_service.get(), IsConnected())
      .WillRepeatedly(Return(true));
  manager()->UpdateService(mock_service);
  // We can't EXPECT_CALL(..., MakeFavorite), because that requires us
  // to mock out MakeFavorite. And mocking that out would break the
  // SortServices test. (crosbug.com/23370)
  EXPECT_TRUE(mock_service->favorite());
  EXPECT_TRUE(mock_service->auto_connect());
}

TEST_F(ManagerTest, UpdateServiceConnectedPersistFavorite) {
  // This tests the case where the user connects to a service that is
  // currently associated with a profile.  We want to make sure that the
  // favorite flag is set and that the flag is saved to the current
  // profile.
  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));
  manager()->RegisterService(mock_service);
  EXPECT_FALSE(mock_service->favorite());
  EXPECT_FALSE(mock_service->auto_connect());

  scoped_refptr<MockProfile> profile(
      new MockProfile(control_interface(), manager(), ""));

  mock_service->set_profile(profile);
  EXPECT_CALL(*mock_service, IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*profile,
              UpdateService(static_cast<ServiceRefPtr>(mock_service)));
  manager()->UpdateService(mock_service);
  // We can't EXPECT_CALL(..., MakeFavorite), because that requires us
  // to mock out MakeFavorite. And mocking that out would break the
  // SortServices test. (crosbug.com/23370)
  EXPECT_TRUE(mock_service->favorite());
  EXPECT_TRUE(mock_service->auto_connect());
  // This releases the ref on the mock profile.
  mock_service->set_profile(NULL);
}

TEST_F(ManagerTest, SaveSuccessfulService) {
  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));
  AdoptProfile(manager(), profile);
  scoped_refptr<MockService> service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  // Re-cast this back to a ServiceRefPtr, so EXPECT arguments work correctly.
  ServiceRefPtr expect_service(service.get());

  EXPECT_CALL(*profile.get(), ConfigureService(expect_service))
      .WillOnce(Return(false));
  manager()->RegisterService(service);

  EXPECT_CALL(*service.get(), state())
      .WillRepeatedly(Return(Service::kStateConnected));
  EXPECT_CALL(*service.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*profile.get(), AdoptService(expect_service))
      .WillOnce(Return(true));
  manager()->UpdateService(service);
}

TEST_F(ManagerTest, EnumerateProfiles) {
  vector<string> profile_paths;
  for (size_t i = 0; i < 10; i++) {
    scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));
    profile_paths.push_back(base::StringPrintf("/profile/%zd", i));
    EXPECT_CALL(*profile.get(), GetRpcIdentifier())
        .WillOnce(Return(profile_paths.back()));
    AdoptProfile(manager(), profile);
  }

  Error error;
  vector<string> returned_paths = manager()->EnumerateProfiles(&error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(profile_paths.size(), returned_paths.size());
  for (size_t i = 0; i < profile_paths.size(); i++) {
    EXPECT_EQ(profile_paths[i], returned_paths[i]);
  }
}

TEST_F(ManagerTest, AutoConnectOnRegister) {
  MockServiceRefPtr service = MakeAutoConnectableService();
  EXPECT_CALL(*service.get(), AutoConnect());
  manager()->RegisterService(service);
  dispatcher()->DispatchPendingEvents();
}

TEST_F(ManagerTest, AutoConnectOnUpdate) {
  MockServiceRefPtr service1 = MakeAutoConnectableService();
  service1->set_priority(1);
  MockServiceRefPtr service2 = MakeAutoConnectableService();
  service2->set_priority(2);
  manager()->RegisterService(service1);
  manager()->RegisterService(service2);
  dispatcher()->DispatchPendingEvents();

  EXPECT_CALL(*service1.get(), AutoConnect());
  EXPECT_CALL(*service2.get(), state())
      .WillRepeatedly(Return(Service::kStateFailure));
  EXPECT_CALL(*service2.get(), IsFailed())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service2.get(), IsConnected())
      .WillRepeatedly(Return(false));
  manager()->UpdateService(service2);
  dispatcher()->DispatchPendingEvents();
}

TEST_F(ManagerTest, AutoConnectOnDeregister) {
  MockServiceRefPtr service1 = MakeAutoConnectableService();
  service1->set_priority(1);
  MockServiceRefPtr service2 = MakeAutoConnectableService();
  service2->set_priority(2);
  manager()->RegisterService(service1);
  manager()->RegisterService(service2);
  dispatcher()->DispatchPendingEvents();

  EXPECT_CALL(*service1.get(), AutoConnect());
  manager()->DeregisterService(service2);
  dispatcher()->DispatchPendingEvents();
}

TEST_F(ManagerTest, RecheckPortal) {
  EXPECT_CALL(*mock_devices_[0].get(), RequestPortalDetection())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_devices_[1].get(), RequestPortalDetection())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_devices_[2].get(), RequestPortalDetection())
      .Times(0);

  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  manager()->RegisterDevice(mock_devices_[2]);

  manager()->RecheckPortal(NULL);
}

TEST_F(ManagerTest, RecheckPortalOnService) {
  MockServiceRefPtr service = new NiceMock<MockService>(control_interface(),
                                                        dispatcher(),
                                                        metrics(),
                                                        manager());
  EXPECT_CALL(*mock_devices_[0].get(),
              IsConnectedToService(IsRefPtrTo(service)))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_devices_[1].get(),
              IsConnectedToService(IsRefPtrTo(service)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_devices_[1].get(), RestartPortalDetection())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_devices_[2].get(), IsConnectedToService(_))
      .Times(0);

  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  manager()->RegisterDevice(mock_devices_[2]);

  manager()->RecheckPortalOnService(service);
}

TEST_F(ManagerTest, GetDefaultService) {
  EXPECT_FALSE(manager()->GetDefaultService().get());

  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  manager()->RegisterService(mock_service);
  EXPECT_FALSE(manager()->GetDefaultService().get());

  scoped_refptr<MockConnection> mock_connection(
      new NiceMock<MockConnection>(device_info_.get()));
  mock_service->set_mock_connection(mock_connection);
  EXPECT_EQ(mock_service.get(), manager()->GetDefaultService().get());

  mock_service->set_mock_connection(NULL);
  manager()->DeregisterService(mock_service);
}

TEST_F(ManagerTest, GetServiceWithGUID) {
  scoped_refptr<MockService> mock_service0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  scoped_refptr<MockService> mock_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  EXPECT_CALL(*mock_service0.get(), Configure(_, _))
      .Times(0);
  EXPECT_CALL(*mock_service1.get(), Configure(_, _))
      .Times(0);

  manager()->RegisterService(mock_service0);
  manager()->RegisterService(mock_service1);

  const string kGUID0 = "GUID0";
  const string kGUID1 = "GUID1";

  {
    Error error;
    ServiceRefPtr service = manager()->GetServiceWithGUID(kGUID0, &error);
    EXPECT_FALSE(error.IsSuccess());
    EXPECT_FALSE(service);
  }

  KeyValueStore args;
  args.SetString(flimflam::kGuidProperty, kGUID1);

  {
    Error error;
    ServiceRefPtr service = manager()->GetService(args, &error);
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_FALSE(service);
  }

  mock_service0->set_guid(kGUID0);
  mock_service1->set_guid(kGUID1);

  {
    Error error;
    ServiceRefPtr service = manager()->GetServiceWithGUID(kGUID0, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(mock_service0.get(), service.get());
  }

  {
    Error error;
    EXPECT_CALL(*mock_service1.get(), Configure(_, &error))
        .Times(1);
    ServiceRefPtr service = manager()->GetService(args, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(mock_service1.get(), service.get());
  }

  manager()->DeregisterService(mock_service0);
  manager()->DeregisterService(mock_service1);
}


TEST_F(ManagerTest, CalculateStateOffline) {
  MockMetrics mock_metrics;
  manager()->set_metrics(&mock_metrics);
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(_))
      .Times(AnyNumber());
  scoped_refptr<MockService> mock_service0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  scoped_refptr<MockService> mock_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  EXPECT_CALL(*mock_service0.get(), IsConnected())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_service1.get(), IsConnected())
      .WillRepeatedly(Return(false));

  manager()->RegisterService(mock_service0);
  manager()->RegisterService(mock_service1);

  EXPECT_EQ("offline", manager()->CalculateState(NULL));

  manager()->DeregisterService(mock_service0);
  manager()->DeregisterService(mock_service1);
}

TEST_F(ManagerTest, CalculateStateOnline) {
  MockMetrics mock_metrics;
  manager()->set_metrics(&mock_metrics);
  EXPECT_CALL(mock_metrics, NotifyDefaultServiceChanged(_))
      .Times(AnyNumber());
  scoped_refptr<MockService> mock_service0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  scoped_refptr<MockService> mock_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager()));

  EXPECT_CALL(*mock_service0.get(), IsConnected())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_service1.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_service0.get(), state())
      .WillRepeatedly(Return(Service::kStateIdle));
  EXPECT_CALL(*mock_service1.get(), state())
      .WillRepeatedly(Return(Service::kStateConnected));

  manager()->RegisterService(mock_service0);
  manager()->RegisterService(mock_service1);
  CompleteServiceSort();

  EXPECT_EQ("online", manager()->CalculateState(NULL));

  manager()->DeregisterService(mock_service0);
  manager()->DeregisterService(mock_service1);
}

TEST_F(ManagerTest, StartupPortalList) {
  // Simulate loading value from the default profile.
  const string kProfileValue("wifi,vpn");
  manager()->props_.check_portal_list = kProfileValue;

  EXPECT_EQ(kProfileValue, manager()->GetCheckPortalList(NULL));
  EXPECT_TRUE(manager()->IsPortalDetectionEnabled(Technology::kWifi));
  EXPECT_FALSE(manager()->IsPortalDetectionEnabled(Technology::kCellular));

  const string kStartupValue("cellular,ethernet");
  manager()->SetStartupPortalList(kStartupValue);
  // Ensure profile value is not overwritten, so when we save the default
  // profile, the correct value will still be written.
  EXPECT_EQ(kProfileValue, manager()->props_.check_portal_list);

  // However we should read back a different list.
  EXPECT_EQ(kStartupValue, manager()->GetCheckPortalList(NULL));
  EXPECT_FALSE(manager()->IsPortalDetectionEnabled(Technology::kWifi));
  EXPECT_TRUE(manager()->IsPortalDetectionEnabled(Technology::kCellular));

  const string kRuntimeValue("ppp");
  // Setting a runtime value over the control API should overwrite both
  // the profile value and what we read back.
  Error error;
  manager()->mutable_store()->SetStringProperty(
      flimflam::kCheckPortalListProperty,
      kRuntimeValue,
      &error);
  ASSERT_TRUE(error.IsSuccess());
  EXPECT_EQ(kRuntimeValue, manager()->GetCheckPortalList(NULL));
  EXPECT_EQ(kRuntimeValue, manager()->props_.check_portal_list);
  EXPECT_FALSE(manager()->IsPortalDetectionEnabled(Technology::kCellular));
  EXPECT_TRUE(manager()->IsPortalDetectionEnabled(Technology::kPPP));
}

TEST_F(ManagerTest, EnableTechnology) {
  Error error(Error::kOperationInitiated);
  ResultCallback callback;
  manager()->EnableTechnology(flimflam::kTypeEthernet, &error, callback);
  EXPECT_TRUE(error.IsSuccess());

  ON_CALL(*mock_devices_[0], technology())
      .WillByDefault(Return(Technology::kEthernet));

  manager()->RegisterDevice(mock_devices_[0]);

  // Device is enabled, so expect operation is successful.
  mock_devices_[0]->enabled_ = true;
  error.Populate(Error::kOperationInitiated);
  manager()->EnableTechnology(flimflam::kTypeEthernet, &error, callback);
  EXPECT_TRUE(error.IsSuccess());

  // Device is disabled, so expect operation in progress.
  mock_devices_[0]->enabled_ = false;
  EXPECT_CALL(*mock_devices_[0], SetEnabledPersistent(true, _, _));
  error.Populate(Error::kOperationInitiated);
  manager()->EnableTechnology(flimflam::kTypeEthernet, &error, callback);
  EXPECT_TRUE(error.IsOngoing());
}

TEST_F(ManagerTest, DisableTechnology) {
  Error error(Error::kOperationInitiated);
  ResultCallback callback;
  manager()->DisableTechnology(flimflam::kTypeEthernet, &error, callback);
  EXPECT_TRUE(error.IsSuccess());

  ON_CALL(*mock_devices_[0], technology())
      .WillByDefault(Return(Technology::kEthernet));

  manager()->RegisterDevice(mock_devices_[0]);

  // Device is disabled, so expect operation is successful.
  error.Populate(Error::kOperationInitiated);
  manager()->DisableTechnology(flimflam::kTypeEthernet, &error, callback);
  EXPECT_TRUE(error.IsSuccess());

  // Device is enabled, so expect operation in progress.
  EXPECT_CALL(*mock_devices_[0], SetEnabledPersistent(false, _, _));
  mock_devices_[0]->enabled_ = true;
  error.Populate(Error::kOperationInitiated);
  manager()->DisableTechnology(flimflam::kTypeEthernet, &error, callback);
  EXPECT_TRUE(error.IsOngoing());
}


}  // namespace shill
