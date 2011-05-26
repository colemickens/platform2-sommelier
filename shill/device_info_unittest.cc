// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <glib.h>

#include <base/callback_old.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_control.h"
#include "shill/device_info.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/rtnl_handler.h"

namespace shill {
using ::testing::Test;
using ::testing::_;
using ::testing::NiceMock;

class DeviceInfoTest : public Test {
 public:
  DeviceInfoTest()
      : manager_(&control_interface_, &dispatcher_),
        device_info_(&control_interface_, &dispatcher_, &manager_),
        factory_(this) {
  }
  base::hash_map<int, scoped_refptr<Device> > *DeviceInfoDevices() {
    return &device_info_.devices_;
  }
 protected:
  MockControl control_interface_;
  Manager manager_;
  DeviceInfo device_info_;
  EventDispatcher dispatcher_;
  ScopedRunnableMethodFactory<DeviceInfoTest> factory_;
};

TEST_F(DeviceInfoTest, DeviceEnumeration) {
  // TODO(cmasone): Make this unit test use message loop primitives.

  // Start our own private device_info
  device_info_.Start();
  RTNLHandler::GetInstance()->Start(&dispatcher_);

  // Peek in at the map of devices
  base::hash_map<int, scoped_refptr<Device> > *device_map = DeviceInfoDevices();

  // Crank the glib main loop a few times
  for (int main_loop_count = 0;
       main_loop_count < 6 && device_map->size() == 0;
       ++main_loop_count)
    g_main_context_iteration(NULL, TRUE);

  // The test machine must have a device or two...
  EXPECT_GT(device_map->size(), 0);

  device_info_.Stop();
  RTNLHandler::GetInstance()->Stop();
  // TODO(pstew): Create fake devices (simulators?) so we can do hard tests
}

TEST_F(DeviceInfoTest, DeviceEnumerationReverse) {
  // TODO(cmasone): Make this unit test use message loop primitives.

  // Start our own private device_info _after_ RTNLHandler has been started
  RTNLHandler::GetInstance()->Start(&dispatcher_);
  device_info_.Start();

  // Peek in at the map of devices
  base::hash_map<int, scoped_refptr<Device> > *device_map = DeviceInfoDevices();

  // Crank the glib main loop a few times
  for (int main_loop_count = 0;
       main_loop_count < 6 && device_map->size() == 0;
       ++main_loop_count)
    g_main_context_iteration(NULL, TRUE);

  // The test machine must have a device or two...
  EXPECT_GT(device_map->size(), 0);

  RTNLHandler::GetInstance()->Stop();
  device_info_.Stop();
  // TODO(pstew): Create fake devices (simulators?) so we can do hard tests
}

}  // namespace shill
