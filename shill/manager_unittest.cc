// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <set>

#include <glib.h>

#include <base/logging.h>
#include <base/memory/scoped_temp_dir.h>
#include <base/stl_util-inl.h>
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
#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/mock_wifi.h"
#include "shill/property_store_unittest.h"
#include "shill/service_under_test.h"
#include "shill/wifi_service.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ContainerEq;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Test;

class ManagerTest : public PropertyStoreTest {
 public:
  ManagerTest()
      : mock_wifi_(new NiceMock<MockWiFi>(control_interface(),
                                          dispatcher(),
                                          manager(),
                                          "wifi0",
                                          "addr4",
                                          4)),
        device_info_(new NiceMock<MockDeviceInfo>(
            control_interface(),
            reinterpret_cast<EventDispatcher*>(NULL),
            reinterpret_cast<Manager*>(NULL))),
        manager_adaptor_(new NiceMock<ManagerMockAdaptor>()) {
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     manager(),
                                                     "null0",
                                                     "addr0",
                                                     0));
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     manager(),
                                                     "null1",
                                                     "addr1",
                                                     1));
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     manager(),
                                                     "null2",
                                                     "addr2",
                                                     2));
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
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
    manager->CreateProfile(name, &error);
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
    manager->PushProfile(name, &error);
    return error.type();
  }
 protected:
  scoped_refptr<MockWiFi> mock_wifi_;
  vector<scoped_refptr<MockDevice> > mock_devices_;
  scoped_ptr<MockDeviceInfo> device_info_;

  // This pointer is owned by the manager, and only tracked here for EXPECT*()
  ManagerMockAdaptor *manager_adaptor_;
};

bool ManagerTest::ServiceOrderIs(ServiceRefPtr svc0, ServiceRefPtr svc1) {
  return (svc0.get() == manager()->services_[0].get() &&
          svc1.get() == manager()->services_[1].get());
}

TEST_F(ManagerTest, Contains) {
  EXPECT_TRUE(manager()->store().Contains(flimflam::kStateProperty));
  EXPECT_FALSE(manager()->store().Contains(""));
}

TEST_F(ManagerTest, DeviceRegistration) {
  ON_CALL(*mock_devices_[0].get(), TechnologyIs(Technology::kEthernet))
      .WillByDefault(Return(true));
  ON_CALL(*mock_devices_[1].get(), TechnologyIs(Technology::kWifi))
      .WillByDefault(Return(true));
  ON_CALL(*mock_devices_[2].get(), TechnologyIs(Technology::kCellular))
      .WillByDefault(Return(true));

  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  manager()->RegisterDevice(mock_devices_[2]);

  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));
  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));
  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[2], Technology::kCellular));
}

TEST_F(ManagerTest, DeviceRegistrationAndStart) {
  manager()->running_ = true;
  mock_devices_[0]->powered_ = true;
  mock_devices_[1]->powered_ = false;
  EXPECT_CALL(*mock_devices_[0].get(), Start())
      .Times(1);
  EXPECT_CALL(*mock_devices_[1].get(), Start())
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
  ON_CALL(*mock_devices_[0].get(), TechnologyIs(Technology::kEthernet))
      .WillByDefault(Return(true));
  ON_CALL(*mock_devices_[1].get(), TechnologyIs(Technology::kWifi))
      .WillByDefault(Return(true));

  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);

  ASSERT_TRUE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));
  ASSERT_TRUE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));

  EXPECT_CALL(*mock_devices_[0].get(), Stop());
  manager()->DeregisterDevice(mock_devices_[0]);
  EXPECT_FALSE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));

  EXPECT_CALL(*mock_devices_[1].get(), Stop());
  manager()->DeregisterDevice(mock_devices_[1]);
  EXPECT_FALSE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));
}

TEST_F(ManagerTest, ServiceRegistration) {
  // It's much easier and safer to use a real GLib for this test.
  GLib glib;
  Manager manager(control_interface(),
                  dispatcher(),
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
                                &manager));
  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
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
                                                &manager));
    ASSERT_TRUE(profile->AdoptService(service1));
    ASSERT_TRUE(profile->ContainsService(service1));
  }  // Force destruction of service1.

  ServiceRefPtr service2(new ServiceUnderTest(control_interface(),
                                              dispatcher(),
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
                                                &manager));
    ASSERT_TRUE(profile->AdoptService(service1));
    ASSERT_TRUE(profile->ContainsService(service1));
  }  // Force destruction of service1.
  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                &manager));
  EXPECT_CALL(*mock_service2.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(mock_service2->UniqueName()));
  manager.RegisterService(mock_service2);
  EXPECT_NE(mock_service2->profile().get(), profile.get());
  manager.Stop();
}

TEST_F(ManagerTest, GetProperties) {
  ProfileRefPtr profile(new MockProfile(control_interface(), manager(), ""));
  AdoptProfile(manager(), profile);
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
  ProfileRefPtr profile(new MockProfile(control_interface(), manager(), ""));
  AdoptProfile(manager(), profile);
  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  {
    map<string, ::DBus::Variant> props;
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(manager()->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kDevicesProperty) == props.end());
    Strings devices =
        props[flimflam::kDevicesProperty].operator vector<string>();
    EXPECT_EQ(2, devices.size());
  }
}

TEST_F(ManagerTest, MoveService) {
  Manager manager(control_interface(),
                  dispatcher(),
                  glib(),
                  run_path(),
                  storage_path(),
                  string());
  scoped_refptr<MockService> s2(new MockService(control_interface(),
                                                dispatcher(),
                                                &manager));
  // Inject an actual profile, backed by a fake StoreInterface
  {
    Profile::Identifier id("irrelevant");
    ProfileRefPtr profile(
        new Profile(control_interface(), &manager, id, "", false));
    MockStore *storage = new MockStore;
    // Say we don't have |s2| the first time asked, then that we do.
    EXPECT_CALL(*storage, ContainsGroup(s2->GetStorageIdentifier()))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*storage, Flush())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    profile->set_storage(storage);
    AdoptProfile(&manager, profile);
  }
  // Create a profile that already has |s2| in it.
  ProfileRefPtr profile(new EphemeralProfile(control_interface(), &manager));
  profile->AdoptService(s2);

  // Now, move the Service |s2| to another profile.
  EXPECT_CALL(*s2.get(), Save(_)).WillOnce(Return(true));
  ASSERT_TRUE(manager.MoveServiceToProfile(s2, manager.ActiveProfile()));

  // Force destruction of the original Profile, to ensure that the Service
  // is kept alive and populated with data.
  profile = NULL;
  ASSERT_TRUE(manager.ActiveProfile()->ContainsService(s2));
  manager.Stop();
}

TEST_F(ManagerTest, CreateProfile) {
  // It's much easier to use real Glib here since we want the storage
  // side-effects.
  GLib glib;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  Manager manager(control_interface(),
                  dispatcher(),
                  &glib,
                  run_path(),
                  storage_path(),
                  temp_dir.path().value());

  // Invalid name should be rejected.
  EXPECT_EQ(Error::kInvalidArguments, TestCreateProfile(&manager, ""));

  // Valid name is still rejected because we can't create a profile
  // that doesn't have a user component.  Such profile names are
  // reserved for the single DefaultProfile the manager creates
  // at startup.
  EXPECT_EQ(Error::kInvalidArguments, TestCreateProfile(&manager, "valid"));

  // We should succeed in creating a valid user profile.
  const char kProfile[] = "~user/profile";
  EXPECT_EQ(Error::kSuccess, TestCreateProfile(&manager, kProfile));

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
                  &glib,
                  run_path(),
                  storage_path(),
                  temp_dir.path().value());

  // Pushing an invalid profile should fail.
  EXPECT_EQ(Error::kInvalidArguments, TestPushProfile(&manager, ""));

  // Pushing a default profile name should fail.
  EXPECT_EQ(Error::kInvalidArguments, TestPushProfile(&manager, "default"));

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
  EXPECT_EQ(kProfile1, "~" + manager.GetActiveProfileName(&error));

  // Make sure a profile name that doesn't exist fails.
  const char kProfile2Id[] = "profile2";
  const string kProfile2 = base::StringPrintf("~user/%s", kProfile2Id);
  EXPECT_EQ(Error::kNotFound, TestPushProfile(&manager, kProfile2));

  // Create a new service, with a specific storage name.
  scoped_refptr<MockService> service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                &manager));
  const char kServiceName[] = "service_storage_name";
  EXPECT_CALL(*service.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(kServiceName));
  EXPECT_CALL(*service.get(), Load(_))
      .WillRepeatedly(Return(true));

  // Add this service to the manager -- it should end up in the ephemeral
  // profile.
  manager.RegisterService(service);
  ASSERT_EQ(manager.ephemeral_profile_, service->profile());

  // Create storage for a profile that contains the service storage name.
  ASSERT_TRUE(CreateBackingStoreForService(&temp_dir, kProfile2Id,
                                           kServiceName));

  // When we push the profile, the service should move away from the
  // ephemeral profile to this new profile since it has an entry for
  // this service.
  EXPECT_EQ(Error::kSuccess, TestPushProfile(&manager, kProfile2));
  EXPECT_NE(manager.ephemeral_profile_, service->profile());
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
  EXPECT_EQ(manager.ephemeral_profile_, service->profile());

  // Pop the remaining two services off the stack.
  EXPECT_EQ(Error::kSuccess, TestPopAnyProfile(&manager));
  EXPECT_EQ(Error::kSuccess, TestPopAnyProfile(&manager));

  // Next pop should fail with "stack is empty".
  EXPECT_EQ(Error::kNotFound, TestPopAnyProfile(&manager));
}

TEST_F(ManagerTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(manager()->mutable_store(),
                                            flimflam::kOfflineModeProperty,
                                            PropertyStoreTest::kBoolV,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(manager()->mutable_store(),
                                            flimflam::kCountryProperty,
                                            PropertyStoreTest::kStringV,
                                            &error));
  }
  // Attempt to write with value of wrong type should return InvalidArgs.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(manager()->mutable_store(),
                                             flimflam::kCountryProperty,
                                             PropertyStoreTest::kBoolV,
                                             &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(manager()->mutable_store(),
                                             flimflam::kOfflineModeProperty,
                                             PropertyStoreTest::kStringV,
                                             &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  // Attempt to write R/O property should return InvalidArgs.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(
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
    EXPECT_CALL(*mock_devices_[0], TechnologyIs(Technology::kWifi))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_devices_[0], Scan(_));
    EXPECT_CALL(*mock_devices_[1], TechnologyIs(Technology::kWifi))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_devices_[1], Scan(_)).Times(0);
    manager()->RequestScan(flimflam::kTypeWifi, &error);
  }

  {
    Error error;
    manager()->RequestScan("bogus_device_type", &error);
    EXPECT_EQ(Error::kInvalidArguments, error.type());
  }
}

TEST_F(ManagerTest, GetWifiServiceNoDevice) {
  KeyValueStore args;
  Error e;
  manager()->GetWifiService(args, &e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("no wifi devices available", e.message());
}

TEST_F(ManagerTest, GetWifiService) {
  KeyValueStore args;
  Error e;
  WiFiServiceRefPtr wifi_service;

  manager()->RegisterDevice(mock_wifi_);
  EXPECT_CALL(*mock_wifi_, GetService(_, _))
      .WillRepeatedly(Return(wifi_service));
  manager()->GetWifiService(args, &e);
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
                                manager()));
  scoped_refptr<MockService> mock_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                manager()));

  manager()->RegisterService(mock_service0);
  manager()->RegisterService(mock_service1);

  // Services should already be sorted by UniqueName
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Asking explictly to sort services should not change anything
  manager()->SortServices();
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Two otherwise equal services should be reordered by strength
  mock_service1->set_strength(1);
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  // Security
  mock_service0->set_security_level(1);
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Technology
  EXPECT_CALL(*mock_service0.get(), TechnologyIs(Technology::kWifi))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_service1.get(), TechnologyIs(Technology::kEthernet))
      .WillRepeatedly(Return(true));
  // NB: Redefine default (false) return values so we don't use the default rule
  // which makes the logs noisier
  EXPECT_CALL(*mock_service0.get(), TechnologyIs(Ne(Technology::kWifi)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_service1.get(), TechnologyIs(Ne(Technology::kEthernet)))
      .WillRepeatedly(Return(false));

  Error error;
  manager()->SetTechnologyOrder(string(flimflam::kTypeEthernet) + "," +
                                string(flimflam::kTypeWifi), &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  manager()->SetTechnologyOrder(string(flimflam::kTypeWifi) + "," +
                                string(flimflam::kTypeEthernet), &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Priority.
  mock_service0->set_priority(1);
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Favorite
  mock_service1->MakeFavorite();
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  // Connecting.
  EXPECT_CALL(*mock_service0.get(), state())
      .WillRepeatedly(Return(Service::kStateAssociating));
  EXPECT_CALL(*mock_service0.get(), IsConnecting())
      .WillRepeatedly(Return(true));
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Connected.
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
  scoped_refptr<MockService> mock_service0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                manager()));
  scoped_refptr<MockService> mock_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                manager()));

  scoped_refptr<MockConnection> mock_connection0(
      new NiceMock<MockConnection>(device_info_.get()));
  scoped_refptr<MockConnection> mock_connection1(
      new NiceMock<MockConnection>(device_info_.get()));

  manager()->RegisterService(mock_service0);
  manager()->RegisterService(mock_service1);

  mock_service0->connection_ = mock_connection0;
  mock_service1->connection_ = mock_connection1;

  EXPECT_CALL(*mock_connection0.get(), SetIsDefault(true));
  manager()->SortServices();

  mock_service1->set_priority(1);
  EXPECT_CALL(*mock_connection0.get(), SetIsDefault(false));
  EXPECT_CALL(*mock_connection1.get(), SetIsDefault(true));
  manager()->SortServices();

  EXPECT_CALL(*mock_connection0.get(), SetIsDefault(true));
  mock_service1->connection_ = NULL;
  manager()->DeregisterService(mock_service1);

  mock_service0->connection_ = NULL;
  manager()->DeregisterService(mock_service0);
}

TEST_F(ManagerTest, AvailableTechnologies) {
  mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                   dispatcher(),
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
                                manager()));
  scoped_refptr<MockService> connected_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                manager()));
  scoped_refptr<MockService> disconnected_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                manager()));
  scoped_refptr<MockService> disconnected_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
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
                                manager()));
  scoped_refptr<MockService> disconnected_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
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
  Error error;
  EXPECT_THAT(manager()->DefaultTechnology(&error), StrEq(""));


  manager()->RegisterService(connected_service);
  // Connected service should be brought to the front now.
  string expected_technology =
      Technology::NameFromIdentifier(Technology::kWifi);
  EXPECT_THAT(manager()->DefaultTechnology(&error), StrEq(expected_technology));
}

TEST_F(ManagerTest, DisconnectServicesOnStop) {
  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                manager()));
  manager()->RegisterService(mock_service);
  EXPECT_CALL(*mock_service.get(), Disconnect(_)).Times(1);
  manager()->Stop();
}

TEST_F(ManagerTest, UpdateServiceConnected) {
  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
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

TEST_F(ManagerTest, SaveSuccessfulService) {
  scoped_refptr<MockProfile> profile(
      new StrictMock<MockProfile>(control_interface(), manager(), ""));
  AdoptProfile(manager(), profile);
  scoped_refptr<MockService> service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
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

}  // namespace shill
