// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/device.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using std::vector;

namespace apmanager {

namespace {

const char kDeviceName[] = "phy0";
const Device::WiFiInterface kApModeInterface0 = {
    "uap0", kDeviceName, 1, NL80211_IFTYPE_AP
};
const Device::WiFiInterface kApModeInterface1 = {
    "uap1", kDeviceName, 2, NL80211_IFTYPE_AP
};
const Device::WiFiInterface kManagedModeInterface0 = {
    "wlan0", kDeviceName, 3, NL80211_IFTYPE_STATION
};
const Device::WiFiInterface kManagedModeInterface1 = {
    "wlan1", kDeviceName, 4, NL80211_IFTYPE_STATION
};
const Device::WiFiInterface kMonitorModeInterface = {
    "monitor0", kDeviceName, 5, NL80211_IFTYPE_MONITOR
};

}  // namespace

class DeviceTest : public testing::Test {
 public:
  DeviceTest() : device_(new Device(kDeviceName)) {}

  void VerifyInterfaceList(
      const vector<Device::WiFiInterface>& interface_list) {
    EXPECT_EQ(interface_list.size(), device_->interface_list_.size());
    for (size_t i = 0; i < interface_list.size(); i++) {
      EXPECT_TRUE(interface_list[i].Equals(device_->interface_list_[i]));
    }
  }

  void VerifyPreferredApInterface(const std::string& interface_name) {
    EXPECT_EQ(interface_name, device_->GetPreferredApInterface());
  }

 protected:
  scoped_refptr<Device> device_;
};

TEST_F(DeviceTest, RegisterInterface) {
  vector<Device::WiFiInterface> interface_list;
  interface_list.push_back(kApModeInterface0);
  interface_list.push_back(kManagedModeInterface0);
  interface_list.push_back(kMonitorModeInterface);

  device_->RegisterInterface(kApModeInterface0);
  device_->RegisterInterface(kManagedModeInterface0);
  device_->RegisterInterface(kMonitorModeInterface);

  // Verify result interface list.
  VerifyInterfaceList(interface_list);
}

TEST_F(DeviceTest, DeregisterInterface) {
  vector<Device::WiFiInterface> interface_list;
  interface_list.push_back(kApModeInterface0);
  interface_list.push_back(kManagedModeInterface0);

  // Register all interfaces, then deregister monitor0 and wlan1 interfaces.
  device_->RegisterInterface(kApModeInterface0);
  device_->RegisterInterface(kMonitorModeInterface);
  device_->RegisterInterface(kManagedModeInterface0);
  device_->RegisterInterface(kManagedModeInterface1);
  device_->DeregisterInterface(kMonitorModeInterface);
  device_->DeregisterInterface(kManagedModeInterface1);

  // Verify result interface list.
  VerifyInterfaceList(interface_list);
}

TEST_F(DeviceTest, PreferredAPInterface) {
  // Register a monitor mode interface, no preferred AP mode interface.
  device_->RegisterInterface(kMonitorModeInterface);
  VerifyPreferredApInterface("");

  // Register a managed mode interface, should be set to preferred AP interface.
  device_->RegisterInterface(kManagedModeInterface0);
  VerifyPreferredApInterface(kManagedModeInterface0.iface_name);

  // Register a ap mode interface, should be set to preferred AP interface.
  device_->RegisterInterface(kApModeInterface0);
  VerifyPreferredApInterface(kApModeInterface0.iface_name);

  // Register another ap mode interface "uap1" and managed mode interface
  // "wlan1", preferred AP interface should still be set to the first detected
  // ap mode interface "uap0".
  device_->RegisterInterface(kApModeInterface1);
  device_->RegisterInterface(kManagedModeInterface1);
  VerifyPreferredApInterface(kApModeInterface0.iface_name);

  // Deregister the first ap mode interface, preferred AP interface should be
  // set to the second ap mode interface.
  device_->DeregisterInterface(kApModeInterface0);
  VerifyPreferredApInterface(kApModeInterface1.iface_name);

  // Deregister the second ap mode interface, preferred AP interface should be
  // set the first managed mode interface.
  device_->DeregisterInterface(kApModeInterface1);
  VerifyPreferredApInterface(kManagedModeInterface0.iface_name);

  // Deregister the first managed mode interface, preferred AP interface
  // should be set to the second managed mode interface.
  device_->DeregisterInterface(kManagedModeInterface0);
  VerifyPreferredApInterface(kManagedModeInterface1.iface_name);

  // Deregister the second managed mode interface, preferred AP interface
  // should be set to empty string.
  device_->DeregisterInterface(kManagedModeInterface1);
  VerifyPreferredApInterface("");
}

}  // namespace apmanager
