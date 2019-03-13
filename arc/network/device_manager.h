// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_DEVICE_MANAGER_H_
#define ARC_NETWORK_DEVICE_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "arc/network/address_manager.h"
#include "arc/network/device.h"
#include "arc/network/ipc.pb.h"

namespace arc_networkd {

// Convenience class for managing the collection of host devices and their
// presentation in the container.
class DeviceManager {
 public:
  // |addr_mgr| must not be null.
  DeviceManager(AddressManager* addr_mgr,
                const Device::MessageSink& msg_sink,
                const std::string& arc_device);
  ~DeviceManager();

  // Provides the list of devices the manager should track.
  // Returns the number of devices that will be tracked.
  size_t Reset(const std::set<std::string>& devices);

  // Adds a new device by name. Returns whether the device was successfully
  // added.
  bool Add(const std::string& name);

  // Enable/disable traffic to the legacy Android device.
  // These calls do nothing if multinetworking is enabled.
  bool Enable(const std::string& ifname);
  bool Disable();

  // Returns a new fully configured device.
  // Note this is public mainly for testing and should not be called directly.
  std::unique_ptr<Device> MakeDevice(const std::string& name) const;

 private:
  // Provisions and tracks subnets and addresses.
  AddressManager* addr_mgr_;

  // Enables the devices to send messages to the helper process.
  const Device::MessageSink msg_sink_;

  // Connected devices keyed by the interface name.
  // The legacy device is mapped to the Android interface name.
  std::map<std::string, std::unique_ptr<Device>> devices_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_DEVICE_MANAGER_H_
