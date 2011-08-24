// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/default_profile.h"

#include <map>
#include <string>

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/property_store_unittest.h"

using std::map;
using std::string;
using ::testing::_;

namespace shill {

class DefaultProfileTest : public PropertyStoreTest {
 public:
  DefaultProfileTest()
      : profile_(new DefaultProfile(&control_interface_,
                                    &glib_,
                                    &manager_,
                                    properties_)) {
  }

  virtual ~DefaultProfileTest() {}

 protected:
  ProfileRefPtr profile_;
  GLib glib_;
  Manager::Properties properties_;
};

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
        profile_->store()->SetBoolProperty(flimflam::kOfflineModeProperty,
                                           true,
                                           &error));
  }
}

}  // namespace shill
