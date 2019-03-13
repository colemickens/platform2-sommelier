// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device_manager.h"

#include <utility>

#include <base/strings/stringprintf.h>

namespace arc_networkd {

DeviceManager::DeviceManager(AddressManager* addr_mgr,
                             const Device::MessageSink& msg_sink,
                             const std::string& arc_device)
    : addr_mgr_(addr_mgr), msg_sink_(msg_sink) {
  DCHECK(addr_mgr_);
  Add(arc_device);
}

DeviceManager::~DeviceManager() {}

size_t DeviceManager::Reset(const std::set<std::string>& devices) {
  for (auto it = devices_.begin(); it != devices_.end();) {
    const std::string& name = it->first;
    if (name != kAndroidDevice && name != kAndroidLegacyDevice &&
        devices.find(name) == devices.end()) {
      LOG(INFO) << "Removing device " << name;
      it = devices_.erase(it);
    } else {
      ++it;
    }
  }
  for (const std::string& name : devices) {
    Add(name);
  }
  return devices_.size();
}

bool DeviceManager::Add(const std::string& name) {
  if (name.empty() || devices_.find(name) != devices_.end())
    return false;

  auto device = MakeDevice(name);
  if (!device)
    return false;

  LOG(INFO) << "Adding device " << name;
  devices_.emplace(name, std::move(device));
  return true;
}

bool DeviceManager::Enable(const std::string& ifname) {
  const auto it = devices_.find(kAndroidLegacyDevice);
  if (it == devices_.end()) {
    LOG(WARNING) << "Enable not supported in multinetworking mode";
    return false;
  }

  it->second->Disable();
  if (!ifname.empty())
    it->second->Enable(ifname);
  return true;
}

bool DeviceManager::Disable() {
  return Enable("");
}

std::unique_ptr<Device> DeviceManager::MakeDevice(
    const std::string& name) const {
  DCHECK(!name.empty());

  Device::Options opts;
  std::string host_ifname, guest_ifname;
  AddressManager::Guest guest = AddressManager::Guest::ARC;

  // TODO(garrick): Multicast forwarding should be only on for Ethernet and
  // Wifi.
  if (name == kAndroidLegacyDevice) {
    host_ifname = "arcbr0";
    guest_ifname = "arc0";
    opts.find_ipv6_routes = true;
    opts.fwd_multicast = true;
  } else {
    if (name == kAndroidDevice) {
      host_ifname = "arcbr0";
    } else {
      guest = AddressManager::Guest::ARC_NET;
      host_ifname = base::StringPrintf("arc_%s", name.c_str());
    }
    guest_ifname = name;
    opts.find_ipv6_routes = false;
    opts.fwd_multicast = false;
  }

  auto ipv4_subnet = addr_mgr_->AllocateIPv4Subnet(guest);
  if (!ipv4_subnet) {
    LOG(ERROR) << "Subnet already in use or unavailable. Cannot make device: "
               << name;
    return nullptr;
  }
  auto host_ipv4_addr = ipv4_subnet->AllocateAtOffset(0);
  if (!host_ipv4_addr) {
    LOG(ERROR)
        << "Bridge address already in use or unavailable. Cannot make device: "
        << name;
    return nullptr;
  }
  auto guest_ipv4_addr = ipv4_subnet->AllocateAtOffset(1);
  if (!guest_ipv4_addr) {
    LOG(ERROR)
        << "ARC address already in use or unavailable. Cannot make device: "
        << name;
    return nullptr;
  }

  auto config = std::make_unique<Device::Config>(
      host_ifname, guest_ifname, addr_mgr_->GenerateMacAddress(),
      std::move(ipv4_subnet), std::move(host_ipv4_addr),
      std::move(guest_ipv4_addr));

  return std::make_unique<Device>(name, std::move(config), opts, msg_sink_);
}

}  // namespace arc_networkd
