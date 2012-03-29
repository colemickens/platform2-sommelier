// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/default_profile.h"

#include <map>
#include <string>
#include <vector>

#include <base/file_path.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/key_file_store.h"
#include "shill/glib.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_store.h"
#include "shill/portal_detector.h"
#include "shill/property_store_unittest.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace shill {

class DefaultProfileTest : public PropertyStoreTest {
 public:
  DefaultProfileTest()
      : profile_(new DefaultProfile(control_interface(),
                                    manager(),
                                    FilePath(storage_path()),
                                    DefaultProfile::kDefaultId,
                                    properties_)),
        device_(new MockDevice(control_interface(),
                               dispatcher(),
                               metrics(),
                               manager(),
                               "null0",
                               "addr0",
                               0)) {
  }

  virtual ~DefaultProfileTest() {}

  virtual void SetUp() {
    PropertyStoreTest::SetUp();
    FilePath final_path;
    ASSERT_TRUE(profile_->GetStoragePath(&final_path));
    scoped_ptr<KeyFileStore> storage(new KeyFileStore(&real_glib_));
    storage->set_path(final_path);
    ASSERT_TRUE(storage->Open());
    profile_->set_storage(storage.release());  // Passes ownership.
  }

 protected:
  static const char kTestStoragePath[];

  GLib real_glib_;
  scoped_refptr<DefaultProfile> profile_;
  scoped_refptr<MockDevice> device_;
  Manager::Properties properties_;
};

const char DefaultProfileTest::kTestStoragePath[] = "/no/where";

TEST_F(DefaultProfileTest, GetProperties) {
  Error error(Error::kInvalidProperty, "");
  {
    map<string, ::DBus::Variant> props;
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(profile_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kOfflineModeProperty) == props.end());
    EXPECT_FALSE(props[flimflam::kOfflineModeProperty].reader().get_bool());
  }
  properties_.offline_mode = true;
  {
    map<string, ::DBus::Variant> props;
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(profile_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kOfflineModeProperty) == props.end());
    EXPECT_TRUE(props[flimflam::kOfflineModeProperty].reader().get_bool());
  }
  {
    Error error(Error::kInvalidProperty, "");
    EXPECT_FALSE(
        profile_->mutable_store()->SetBoolProperty(
            flimflam::kOfflineModeProperty,
            true,
            &error));
  }
}

TEST_F(DefaultProfileTest, Save) {
  scoped_ptr<MockStore> storage(new MockStore);
  EXPECT_CALL(*storage.get(), SetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStorageName,
                                        DefaultProfile::kDefaultId))
      .WillOnce(Return(true));
  EXPECT_CALL(*storage.get(), SetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStorageHostName,
                                        ""))
      .WillOnce(Return(true));
  EXPECT_CALL(*storage.get(), SetBool(DefaultProfile::kStorageId,
                                      DefaultProfile::kStorageOfflineMode,
                                      false))
      .WillOnce(Return(true));
  EXPECT_CALL(*storage.get(), SetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStorageCheckPortalList,
                                        ""))
      .WillOnce(Return(true));
  EXPECT_CALL(*storage.get(), SetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStoragePortalURL,
                                        ""))
      .WillOnce(Return(true));
  EXPECT_CALL(*storage.get(),
              SetString(DefaultProfile::kStorageId,
                        DefaultProfile::kStoragePortalCheckInterval,
                        "0"))
      .WillOnce(Return(true));
  EXPECT_CALL(*storage.get(), Flush()).WillOnce(Return(true));

  EXPECT_CALL(*device_.get(), Save(storage.get())).WillOnce(Return(true));
  profile_->set_storage(storage.release());

  manager()->RegisterDevice(device_);
  ASSERT_TRUE(profile_->Save());
  manager()->DeregisterDevice(device_);
}

TEST_F(DefaultProfileTest, LoadManagerDefaultProperties) {
  scoped_ptr<MockStore> storage(new MockStore);
  EXPECT_CALL(*storage.get(), GetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStorageHostName,
                                        _))
      .WillOnce(Return(false));
  EXPECT_CALL(*storage.get(), GetBool(DefaultProfile::kStorageId,
                                      DefaultProfile::kStorageOfflineMode,
                                      _))
      .WillOnce(Return(false));
  EXPECT_CALL(*storage.get(), GetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStorageCheckPortalList,
                                        _))
      .WillOnce(Return(false));
  EXPECT_CALL(*storage.get(), GetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStoragePortalURL,
                                        _))
      .WillOnce(Return(false));
  EXPECT_CALL(*storage.get(),
              GetString(DefaultProfile::kStorageId,
                        DefaultProfile::kStoragePortalCheckInterval,
                        _))
      .WillOnce(Return(false));
  profile_->set_storage(storage.release());

  Manager::Properties manager_props;
  ASSERT_TRUE(profile_->LoadManagerProperties(&manager_props));
  EXPECT_EQ("", manager_props.host_name);
  EXPECT_FALSE(manager_props.offline_mode);
  EXPECT_EQ(PortalDetector::kDefaultCheckPortalList,
            manager_props.check_portal_list);
  EXPECT_EQ(PortalDetector::kDefaultURL, manager_props.portal_url);
  EXPECT_EQ(PortalDetector::kDefaultCheckIntervalSeconds,
            manager_props.portal_check_interval_seconds);
}

TEST_F(DefaultProfileTest, LoadManagerProperties) {
  scoped_ptr<MockStore> storage(new MockStore);
  const string host_name("hostname");
  EXPECT_CALL(*storage.get(), GetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStorageHostName,
                                        _))
      .WillOnce(DoAll(SetArgumentPointee<2>(host_name), Return(true)));
  EXPECT_CALL(*storage.get(), GetBool(DefaultProfile::kStorageId,
                                      DefaultProfile::kStorageOfflineMode,
                                      _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  const string portal_list("technology1,technology2");
  EXPECT_CALL(*storage.get(), GetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStorageCheckPortalList,
                                        _))
      .WillOnce(DoAll(SetArgumentPointee<2>(portal_list), Return(true)));
  const string portal_url("http://www.chromium.org");
  EXPECT_CALL(*storage.get(), GetString(DefaultProfile::kStorageId,
                                        DefaultProfile::kStoragePortalURL,
                                        _))
      .WillOnce(DoAll(SetArgumentPointee<2>(portal_url), Return(true)));
  const string portal_check_interval_string("10");
  const int portal_check_interval_int = 10;
  EXPECT_CALL(*storage.get(),
              GetString(DefaultProfile::kStorageId,
                        DefaultProfile::kStoragePortalCheckInterval,
                        _))
      .WillOnce(DoAll(SetArgumentPointee<2>(portal_check_interval_string),
                      Return(true)));
  profile_->set_storage(storage.release());

  Manager::Properties manager_props;
  ASSERT_TRUE(profile_->LoadManagerProperties(&manager_props));
  EXPECT_EQ(host_name, manager_props.host_name);
  EXPECT_TRUE(manager_props.offline_mode);
  EXPECT_EQ(portal_list, manager_props.check_portal_list);
  EXPECT_EQ(portal_url, manager_props.portal_url);
  EXPECT_EQ(portal_check_interval_int,
            manager_props.portal_check_interval_seconds);
}

TEST_F(DefaultProfileTest, GetStoragePath) {
  FilePath path;
  EXPECT_TRUE(profile_->GetStoragePath(&path));
  EXPECT_EQ(storage_path() + "/default.profile", path.value());
}

}  // namespace shill
