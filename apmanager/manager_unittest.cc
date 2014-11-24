// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/manager.h"

#include <gtest/gtest.h>

#include "apmanager/mock_device.h"

using ::testing::_;
using ::testing::Return;

namespace apmanager {

class ManagerTest : public testing::Test {
 public:
  ManagerTest() : manager_() {}

  void RegisterDevice(scoped_refptr<Device> device) {
    manager_.devices_.push_back(device);
  }

 protected:
  Manager manager_;
};

TEST_F(ManagerTest, GetAvailableDevice) {
  // Register a device without AP support (no preferred AP interface).
  scoped_refptr<MockDevice> device0 = new MockDevice();
  RegisterDevice(device0);

  // No available device for AP operation.
  EXPECT_EQ(nullptr, manager_.GetAvailableDevice());

  // Add AP support to the device.
  const char kTestInterface0[] = "test-interface0";
  device0->SetPreferredApInterface(kTestInterface0);
  EXPECT_EQ(device0, manager_.GetAvailableDevice());

  // Register another device with AP support.
  const char kTestInterface1[] = "test-interface1";
  scoped_refptr<MockDevice> device1 = new MockDevice();
  device1->SetPreferredApInterface(kTestInterface1);
  RegisterDevice(device1);

  // Both devices are idle by default, should return the first added device.
  EXPECT_EQ(device0, manager_.GetAvailableDevice());

  // Set first one to be in used, should return the non-used device.
  device0->SetInUsed(true);
  EXPECT_EQ(device1, manager_.GetAvailableDevice());

  // Both devices are in used, should return a nullptr.
  device1->SetInUsed(true);
  EXPECT_EQ(nullptr, manager_.GetAvailableDevice());
}

TEST_F(ManagerTest, GetDeviceFromInterfaceName) {
  // Register two devices
  scoped_refptr<MockDevice> device0 = new MockDevice();
  scoped_refptr<MockDevice> device1 = new MockDevice();
  RegisterDevice(device0);
  RegisterDevice(device1);

  const char kTestInterface0[] = "test-interface0";
  const char kTestInterface1[] = "test-interface1";

  // interface0 belongs to device0.
  EXPECT_CALL(*device0.get(), InterfaceExists(kTestInterface0))
      .WillOnce(Return(true));
  EXPECT_EQ(device0, manager_.GetDeviceFromInterfaceName(kTestInterface0));

  // interface1 belongs to device1.
  EXPECT_CALL(*device0.get(), InterfaceExists(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*device1.get(), InterfaceExists(kTestInterface1))
      .WillOnce(Return(true));
  EXPECT_EQ(device1, manager_.GetDeviceFromInterfaceName(kTestInterface1));

  // "random" interface is not found.
  EXPECT_CALL(*device1.get(), InterfaceExists(_))
      .WillRepeatedly(Return(false));
  EXPECT_EQ(nullptr, manager_.GetDeviceFromInterfaceName("random"));
}

}  // namespace apmanager
