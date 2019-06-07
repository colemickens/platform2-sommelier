// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_DEVICE_MANAGER_H_
#define ARC_NETWORK_DEVICE_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include <base/memory/weak_ptr.h>
#include <shill/net/rtnl_listener.h>

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

  // Enables/disables the legacy Android device in single-network mode only.
  void EnableLegacyDevice(const std::string& ifname);

  // Returns a new fully configured device.
  // Note this is public mainly for testing and should not be called directly.
  std::unique_ptr<Device> MakeDevice(const std::string& name) const;

 private:
  // Callback from RTNetlink listener, invoked when an interface changes
  // run state.
  void LinkMsgHandler(const shill::RTNLMessage& msg);

  // Listens for RTMGRP_LINK messages and invokes LinkMsgHandler.
  std::unique_ptr<shill::RTNLListener> link_listener_;

  // Provisions and tracks subnets and addresses.
  AddressManager* addr_mgr_;

  // Enables the devices to send messages to the helper process.
  const Device::MessageSink msg_sink_;

  // Connected devices keyed by the interface name.
  // The legacy device is mapped to the Android interface name.
  std::map<std::string, std::unique_ptr<Device>> devices_;
  // Running devices by the host interface name.
  std::set<std::string> running_devices_;

  base::WeakPtrFactory<DeviceManager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_DEVICE_MANAGER_H_
