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
#include "shill/entry.h"
#include "shill/ethernet_service.h"
#include "shill/manager.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/property_store_unittest.h"
#include "shill/shill_event.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Test;

namespace shill {

class ServiceTest : public PropertyStoreTest {
 public:
  static const char kMockServiceName[];
  static const char kMockDeviceRpcId[];
  static const char kEntryName[];
  static const char kProfileName[];

  ServiceTest()
      : service_(new MockService(&control_interface_,
                                 &dispatcher_,
                                 new MockProfile(&control_interface_, &glib_),
                                 new Entry(kEntryName),
                                 kMockServiceName)) {
  }

  virtual ~ServiceTest() {}

 protected:
  scoped_refptr<MockService> service_;
};

const char ServiceTest::kMockServiceName[] = "mock-service";

const char ServiceTest::kMockDeviceRpcId[] = "mock-device-rpc";

const char ServiceTest::kEntryName[] = "entry";

const char ServiceTest::kProfileName[] = "profile";

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

TEST_F(ServiceTest, MoveEntry) {
  // Create a Profile with an Entry in it that should back our Service.
  EntryRefPtr entry(new Entry(kProfileName));
  ProfileRefPtr profile(new Profile(&control_interface_, &glib_));
  profile->entries_[kEntryName] = entry;

  scoped_refptr<MockService> service(new MockService(&control_interface_,
                                                     &dispatcher_,
                                                     profile,
                                                     entry,
                                                     kMockServiceName));
  // Now, move the entry to another profile.
  ProfileRefPtr profile2(new Profile(&control_interface_, &glib_));
  map<string, EntryRefPtr>::iterator it = profile->entries_.find(kEntryName);
  ASSERT_TRUE(it != profile->entries_.end());

  profile2->AdoptEntry(it->first, it->second);
  profile->entries_.erase(it);
  // Force destruction of the original Profile, to ensure that the Entry
  // is kept alive and populated with data.
  profile = NULL;
  {
    Error error(Error::kInvalidProperty, "");
    ::DBus::Error dbus_error;
    map<string, ::DBus::Variant> props;
    bool expected = true;
    service->store()->SetBoolProperty(flimflam::kAutoConnectProperty,
                                      expected,
                                      &error);
    DBusAdaptor::GetProperties(service->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kAutoConnectProperty) == props.end());
    EXPECT_EQ(props[flimflam::kAutoConnectProperty].reader().get_bool(),
              expected);
  }
}

}  // namespace shill
