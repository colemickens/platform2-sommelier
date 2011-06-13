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
#include "shill/ethernet_service.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
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
  ServiceTest() {}
  virtual ~ServiceTest() {}
};

TEST_F(ServiceTest, Dispatch) {
  DeviceRefPtr device(new MockDevice(&control_interface_,
                                     &dispatcher_,
                                     &manager_,
                                     "mock-device",
                                     0));
  ServiceRefPtr service(new MockService(&control_interface_,
                                        &dispatcher_,
                                        device.get(),
                                        "mock-service"));
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service.get(),
                                          flimflam::kSaveCredentialsProperty,
                                          bool_v_,
                                          error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service.get(),
                                          flimflam::kPriorityProperty,
                                          int32_v_,
                                          error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service.get(),
                                          flimflam::kEAPEAPProperty,
                                          string_v_,
                                          error));

  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service.get(),
                                           flimflam::kFavoriteProperty,
                                           bool_v_,
                                           error));
  EXPECT_EQ(invalid_args_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service.get(), "", int16_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service.get(), "", int32_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service.get(),
                                           "",
                                           uint16_v_,
                                           error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service.get(),
                                           "",
                                           uint32_v_,
                                           error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service.get(),
                                           "",
                                           strings_v_,
                                           error));
  EXPECT_EQ(invalid_prop_, error.name());
}

}  // namespace shill
