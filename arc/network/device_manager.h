// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_DEVICE_MANAGER_H_
#define ARC_NETWORK_DEVICE_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "arc/network/device.h"
#include "arc/network/ipc.pb.h"

namespace arc_networkd {

// Convenience class for managing the collection of host devices and their
// presentation in the container.
class DeviceManager {
 public:
  // The legacy Android device is initialized in the constructor.
  explicit DeviceManager(const Device::MessageSink& msg_sink);
  ~DeviceManager();

  // Provides the list of devices the manager should track.
  // Returns the number of devices that will be tracked.
  size_t Reset(const std::set<std::string>& devices);

  // Adds a new device by name. Returns whether the device was successfully
  // added.
  bool Add(const std::string& name);

  // Enable/disable device traffic. This is only applicable for the legacy
  // Android device. Returns false is the device is unknown.
  bool Enable(const std::string& name, const std::string& ifname);
  bool Disable(const std::string& name);

  // Disables all devices.
  void DisableAll();

 private:
  // Enables the devices to send messages to the helper process.
  const Device::MessageSink msg_sink_;

  // Connected devices keyed by the interface name.
  // The legacy device is mapped to the Android interface name.
  std::map<std::string, std::unique_ptr<Device>> devices_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_DEVICE_MANAGER_H_
