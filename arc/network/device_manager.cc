// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device_manager.h"

#include <linux/rtnetlink.h>
#include <net/if.h>

#include <utility>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <shill/net/rtnl_handler.h>

namespace arc_networkd {
namespace {

constexpr const char kArcDevicePrefix[] = "arc_";
constexpr const char kVpnInterfaceHostPattern[] = "tun";
constexpr const char kVpnInterfaceGuestPrefix[] = "cros_";
constexpr std::array<const char*, 2> kEthernetInterfacePrefixes{{"eth", "usb"}};
constexpr std::array<const char*, 2> kWifiInterfacePrefixes{{"wlan", "mlan"}};

bool IsArcDevice(const std::string& ifname) {
  return base::StartsWith(ifname, kArcDevicePrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsHostVpnInterface(const std::string& ifname) {
  return base::StartsWith(ifname, kVpnInterfaceHostPattern,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsEthernetInterface(const std::string& ifname) {
  for (const auto& prefix : kEthernetInterfacePrefixes) {
    if (base::StartsWith(ifname, prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

bool IsWifiInterface(const std::string& ifname) {
  for (const auto& prefix : kWifiInterfacePrefixes) {
    if (base::StartsWith(ifname, prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

}  // namespace

DeviceManager::DeviceManager(std::unique_ptr<ShillClient> shill_client,
                             AddressManager* addr_mgr,
                             const Device::MessageSink& msg_sink,
                             bool is_arc_legacy)
    : shill_client_(std::move(shill_client)),
      addr_mgr_(addr_mgr),
      msg_sink_(msg_sink),
      is_arc_legacy_(is_arc_legacy) {
  DCHECK(shill_client_);
  DCHECK(addr_mgr_);
  link_listener_ = std::make_unique<shill::RTNLListener>(
      shill::RTNLHandler::kRequestLink,
      Bind(&DeviceManager::LinkMsgHandler, weak_factory_.GetWeakPtr()));
  shill::RTNLHandler::GetInstance()->Start(RTMGRP_LINK);
  Add(is_arc_legacy_ ? kAndroidLegacyDevice : kAndroidDevice);

  // In the legacy (single network) ARC++ world, we need to track the
  // default shill interface since it's always routed to the arc bridge.
  // But when mulitple networks are used, this feature is ignored and we
  // need instead to keep track of all the host devices that shill tells us
  // about.
  if (!is_arc_legacy_) {
    shill_client_->RegisterDevicesChangedHandler(base::Bind(
        &DeviceManager::OnDevicesChanged, weak_factory_.GetWeakPtr()));
    shill_client_->ScanDevices(base::Bind(&DeviceManager::OnDevicesChanged,
                                          weak_factory_.GetWeakPtr()));
  }
}

DeviceManager::~DeviceManager() {
  shill_client_->UnregisterDevicesChangedHandler();
}

void DeviceManager::OnGuestStart() {
  if (is_arc_legacy_) {
    shill_client_->RegisterDefaultInterfaceChangedHandler(base::Bind(
        &DeviceManager::OnDefaultInterfaceChanged, weak_factory_.GetWeakPtr()));
  }
}

void DeviceManager::OnGuestStop() {
  if (is_arc_legacy_) {
    shill_client_->UnregisterDefaultInterfaceChangedHandler();
    default_ifname_.clear();
  }
}

bool DeviceManager::Add(const std::string& name) {
  if (name.empty() || devices_.find(name) != devices_.end())
    return false;

  auto device = MakeDevice(name);
  if (!device)
    return false;

  LOG(INFO) << "Adding device " << *device;
  devices_.emplace(name, std::move(device));
  return true;
}

bool DeviceManager::Exists(const std::string& name) const {
  return devices_.find(name) != devices_.end();
}

void DeviceManager::LinkMsgHandler(const shill::RTNLMessage& msg) {
  if (!msg.HasAttribute(IFLA_IFNAME)) {
    LOG(ERROR) << "Link event message does not have IFLA_IFNAME";
    return;
  }

  // Only concerned with host interfaces that were created to support
  // a guest. That said, the arcbr0 device is ignored here since
  // (a) in single network mode, it will be enabled/disabled as the
  // default interface changes, and (b) in multi-network mode there is
  // nothing that is necessary to do for it.
  shill::ByteString b(msg.GetAttribute(IFLA_IFNAME));
  std::string ifname(reinterpret_cast<const char*>(
      b.GetSubstring(0, IFNAMSIZ).GetConstData()));
  if (!IsArcDevice(ifname))
    return;

  bool run = msg.link_status().flags & IFF_RUNNING;
  auto it = running_devices_.find(ifname);
  if (it != running_devices_.end()) {
    // Interface no longer running.
    if (!run) {
      LOG(INFO) << ifname << " is no longer running";
      // If this event is triggered because the guest is going down,
      // then the host device must be disabled. If the device is no longer
      // being tracked then it was physically removed (unplugged) and there
      // is nothing to do.
      auto dev_it = devices_.find(ifname.substr(strlen(kArcDevicePrefix)));
      if (dev_it != devices_.end())
        dev_it->second->Disable();

      running_devices_.erase(it);
    }
    return;
  }

  if (run && it == running_devices_.end()) {
    // Untracked interface now running.
    // As long as the device list is small, this linear search is fine.
    for (auto& d : devices_) {
      auto& dev = d.second;
      if (dev->config().host_ifname() != ifname)
        continue;

      LOG(INFO) << ifname << " is now running";
      running_devices_.insert(ifname);
      dev->Enable(dev->config().guest_ifname());
      return;
    }
  }
}

std::unique_ptr<Device> DeviceManager::MakeDevice(
    const std::string& name) const {
  DCHECK(!name.empty());

  Device::Options opts;
  std::string host_ifname, guest_ifname;
  AddressManager::Guest guest = AddressManager::Guest::ARC;

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
    // Android VPNs and native VPNs use the same "tun%d" name pattern for VPN
    // tun interfaces. To distinguish between both and avoid name collisions
    // native VPN interfaces are not exposed with their exact names inside the
    // ARC network namespace. This additional naming convention is not known to
    // Chrome and ARC has to fix names in ArcNetworkBridge.java when receiving
    // NetworkConfiguration mojo objects from Chrome.
    if (IsHostVpnInterface(guest_ifname)) {
      guest_ifname = kVpnInterfaceGuestPrefix + guest_ifname;
    }
    // TODO(crbug/726815) Also enable |find_ipv6_routes| for cellular networks
    // once IPv6 is enabled on cellular networks in shill.
    opts.find_ipv6_routes =
        IsEthernetInterface(guest_ifname) || IsWifiInterface(guest_ifname);
    opts.fwd_multicast =
        IsEthernetInterface(guest_ifname) || IsWifiInterface(guest_ifname);
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

void DeviceManager::OnDefaultInterfaceChanged(const std::string& ifname) {
  if (ifname == default_ifname_)
    return;

  LOG(INFO) << "Default interface changed from [" << default_ifname_ << "] to ["
            << ifname << "]";
  default_ifname_ = ifname;

  const auto it = devices_.find(kAndroidLegacyDevice);
  if (it == devices_.end()) {
    LOG(DFATAL) << "Expected device not found: " << kAndroidLegacyDevice;
    return;
  }
  it->second->Disable();
  if (!default_ifname_.empty())
    it->second->Enable(default_ifname_);
}

void DeviceManager::OnDevicesChanged(const std::set<std::string>& devices) {
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
}

}  // namespace arc_networkd
