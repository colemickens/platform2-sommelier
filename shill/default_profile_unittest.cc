// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
#include "shill/property_store_unittest.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::Return;

namespace shill {

class DefaultProfileTest : public PropertyStoreTest {
 public:
  DefaultProfileTest()
      : profile_(new DefaultProfile(control_interface(),
                                    manager(),
                                    FilePath(storage_path()),
                                    properties_)),
        device_(new MockDevice(control_interface(),
                               dispatcher(),
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
  ProfileRefPtr profile_;
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
                                        DefaultProfile::kStorageCheckPortalList,
                                        ""))
      .WillOnce(Return(true));
  EXPECT_CALL(*storage.get(), SetBool(DefaultProfile::kStorageId,
                                      DefaultProfile::kStorageOfflineMode,
                                      false))
      .WillOnce(Return(true));
  EXPECT_CALL(*storage.get(), Flush()).WillOnce(Return(true));

  EXPECT_CALL(*device_.get(), Save(storage.get())).WillOnce(Return(true));
  profile_->set_storage(storage.release());

  manager()->RegisterDevice(device_);
  ASSERT_TRUE(profile_->Save());
  manager()->DeregisterDevice(device_);
}

TEST_F(DefaultProfileTest, GetStoragePath) {
  FilePath path;
  EXPECT_TRUE(profile_->GetStoragePath(&path));
  EXPECT_EQ(storage_path() + "/default.profile", path.value());
}

}  // namespace shill
