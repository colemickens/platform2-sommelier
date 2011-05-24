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
  int DeviceInfoFlags() { return device_info_.request_flags_; }
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

  // Start our own private device_info and make sure request flags clear
  device_info_.Start();
  EXPECT_NE(DeviceInfoFlags(), 0);

  // Crank the glib main loop a few times
  for (int main_loop_count = 0;
       main_loop_count < 6 && DeviceInfoFlags() != 0;
       ++main_loop_count)
    g_main_context_iteration(NULL, TRUE);

  EXPECT_EQ(DeviceInfoFlags(), 0);

  // The test machine must have a device or two...
  base::hash_map<int, scoped_refptr<Device> > *device_map = DeviceInfoDevices();
  EXPECT_GT(device_map->size(), 0);

  // TODO(pstew): Create fake devices (simulators?) so we can do hard tests
}

}  // namespace shill
