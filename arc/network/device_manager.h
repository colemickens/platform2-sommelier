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
#include "arc/network/shill_client.h"

namespace arc_networkd {

// Convenience class for managing the collection of host devices and their
// presentation in the container.
class DeviceManager {
 public:
  // |addr_mgr| must not be null.
  DeviceManager(std::unique_ptr<ShillClient> shill_client,
                AddressManager* addr_mgr,
                const Device::MessageSink& msg_sink,
                bool is_arc_legacy);
  ~DeviceManager();

  // Invoked when the ARC guest starts or stops.
  // TODO(garrick): When appropriate, pass in specific guest type.
  void OnGuestStart();
  void OnGuestStop();

  // Returns a new fully configured device.
  // Note this is public mainly for testing and should not be called directly.
  std::unique_ptr<Device> MakeDevice(const std::string& name) const;

  // Returns whether a particular device is traced. For testing only.
  bool Exists(const std::string& name) const;

 private:
  // Adds a new device by name. Returns whether the device was successfully
  // added.
  bool Add(const std::string& name);

  // Callback from ShillClient, invoked whenever the default network
  // interface changes or goes away.
  void OnDefaultInterfaceChanged(const std::string& ifname);

  // Callback from ShillClient, invoked whenever the device list changes.
  // |devices| will contain all devices currently connected to shill
  // (e.g. "eth0", "wlan0", etc).
  void OnDevicesChanged(const std::set<std::string>& devices);

  // Callback from RTNetlink listener, invoked when an interface changes
  // run state.
  void LinkMsgHandler(const shill::RTNLMessage& msg);

  // Listens for RTMGRP_LINK messages and invokes LinkMsgHandler.
  std::unique_ptr<shill::RTNLListener> link_listener_;

  // Receives network device updates and notifications from shill.
  std::unique_ptr<ShillClient> shill_client_;

  // Provisions and tracks subnets and addresses.
  AddressManager* addr_mgr_;

  // Enables the devices to send messages to the helper process.
  const Device::MessageSink msg_sink_;

  // Connected devices keyed by the interface name.
  // The legacy device is mapped to the Android interface name.
  std::map<std::string, std::unique_ptr<Device>> devices_;
  // Running devices by the host interface name.
  std::set<std::string> running_devices_;

  const bool is_arc_legacy_;
  std::string default_ifname_;

  base::WeakPtrFactory<DeviceManager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_DEVICE_MANAGER_H_
