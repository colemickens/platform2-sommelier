// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device_manager.h"

#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <utility>
#include <vector>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <shill/net/rtnl_handler.h>

namespace arc_networkd {
namespace {

constexpr const char kArcDevicePrefix[] = "arc";
constexpr const char kVpnInterfaceHostPattern[] = "tun";
constexpr const char kVpnInterfaceGuestPrefix[] = "cros_";
constexpr std::array<const char*, 2> kEthernetInterfacePrefixes{{"eth", "usb"}};
constexpr std::array<const char*, 2> kWifiInterfacePrefixes{{"wlan", "mlan"}};

// Global compile time switch for the method configuring Ipv6 address for ARC.
// When set to true, arc-networkd will try to generate an address and set onto
// ARC interface (legacy method); when set to false, NDProxy is enabled.
constexpr const bool kFindIpv6RoutesLegacy = true;

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

bool IsMulticastInterface(const std::string& ifname) {
  if (ifname.empty()) {
    return false;
  }

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    // If IPv4 fails, try to open a socket using IPv6.
    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
      LOG(ERROR) << "Unable to create socket";
      return false;
    }
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);
  if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
    PLOG(ERROR) << "SIOCGIFFLAGS failed for " << ifname;
    close(fd);
    return false;
  }

  close(fd);
  return (ifr.ifr_flags & IFF_MULTICAST);
}

}  // namespace

DeviceManager::DeviceManager(std::unique_ptr<ShillClient> shill_client,
                             AddressManager* addr_mgr,
                             Datapath* datapath,
                             HelperProcess* mcast_proxy,
                             HelperProcess* nd_proxy)
    : shill_client_(std::move(shill_client)),
      addr_mgr_(addr_mgr),
      datapath_(datapath),
      mcast_proxy_(mcast_proxy),
      nd_proxy_(nd_proxy) {
  DCHECK(shill_client_);
  DCHECK(addr_mgr_);
  DCHECK(datapath_);
  CHECK(mcast_proxy_);

  link_listener_ = std::make_unique<shill::RTNLListener>(
      shill::RTNLHandler::kRequestLink,
      Bind(&DeviceManager::LinkMsgHandler, weak_factory_.GetWeakPtr()));
  shill::RTNLHandler::GetInstance()->Start(RTMGRP_LINK);

  shill_client_->RegisterDefaultInterfaceChangedHandler(base::Bind(
      &DeviceManager::OnDefaultInterfaceChanged, weak_factory_.GetWeakPtr()));
  shill_client_->RegisterDevicesChangedHandler(
      base::Bind(&DeviceManager::OnDevicesChanged, weak_factory_.GetWeakPtr()));
  shill_client_->ScanDevices(
      base::Bind(&DeviceManager::OnDevicesChanged, weak_factory_.GetWeakPtr()));
  if (nd_proxy) {
    nd_proxy->RegisterDeviceMessageHandler(
        base::Bind(&DeviceManager::OnDeviceMessageFromNDProxy,
                   weak_factory_.GetWeakPtr()));
  }
}

DeviceManager::~DeviceManager() {
  shill_client_->UnregisterDevicesChangedHandler();
  shill_client_->UnregisterDefaultInterfaceChangedHandler();
}

void DeviceManager::RegisterDeviceAddedHandler(GuestMessage::GuestType guest,
                                               const DeviceHandler& handler) {
  add_handlers_[guest] = handler;
}
void DeviceManager::RegisterDeviceRemovedHandler(GuestMessage::GuestType guest,
                                                 const DeviceHandler& handler) {
  rm_handlers_[guest] = handler;
}
void DeviceManager::RegisterDeviceIPv6AddressFoundHandler(
    GuestMessage::GuestType guest, const DeviceHandler& handler) {
  ipv6_handlers_[guest] = handler;
}
void DeviceManager::RegisterDefaultInterfaceChangedHandler(
    GuestMessage::GuestType guest, const NameHandler& handler) {
  default_iface_handlers_[guest] = handler;
}

void DeviceManager::ProcessDevices(const DeviceHandler& handler) {
  for (const auto& d : devices_) {
    handler.Run(d.second.get());
  }
}

void DeviceManager::OnGuestStart(GuestMessage::GuestType guest) {
  for (auto& d : devices_) {
    d.second->OnGuestStart(guest);
  }
}

void DeviceManager::OnGuestStop(GuestMessage::GuestType guest) {
  for (auto& d : devices_) {
    d.second->OnGuestStop(guest);
  }

  default_iface_handlers_.erase(guest);
  ipv6_handlers_.erase(guest);
  rm_handlers_.erase(guest);
  add_handlers_.erase(guest);
}

void DeviceManager::OnDeviceMessageFromNDProxy(const DeviceMessage& msg) {
  const std::string& dev_ifname = msg.dev_ifname();
  LOG_IF(DFATAL, dev_ifname.empty())
      << "Received DeviceMessage w/ empty dev_ifname";
  if (!datapath_->AddIPv6HostRoute(dev_ifname, msg.guest_ip6addr(), 128)) {
    LOG(WARNING) << "Failed to setup the IPv6 route for interface "
                 << dev_ifname;
  }
}

bool DeviceManager::Add(const std::string& name) {
  if (name.empty() || Exists(name))
    return false;

  auto dev = MakeDevice(name);
  if (!dev)
    return false;

  LOG(INFO) << "Adding device " << *dev;
  auto* device = dev.get();
  devices_.emplace(name, std::move(dev));

  if (device->options().ipv6_enabled) {
    if (device->options().find_ipv6_routes_legacy) {
      device->RegisterIPv6SetupHandler(base::Bind(
          &DeviceManager::OnIPv6AddressFound, weak_factory_.GetWeakPtr()));
    } else {
      if (!datapath_->AddIPv6Forwarding(device->ifname(),
                                        device->config().host_ifname())) {
        LOG(ERROR) << "Failed to setup iptables forwarding rule for IPv6 from "
                   << device->ifname() << " to "
                   << device->config().host_ifname();
      }
    }
  }

  for (auto& h : add_handlers_) {
    h.second.Run(device);
  }

  return true;
}

bool DeviceManager::Remove(const std::string& name) {
  auto it = devices_.find(name);
  if (it == devices_.end())
    return false;

  LOG(INFO) << "Removing device " << name;

  if (it->second->options().ipv6_enabled &&
      !it->second->options().find_ipv6_routes_legacy) {
    datapath_->RemoveIPv6Forwarding(it->second->ifname(),
                                    it->second->config().host_ifname());
  }

  if ((nd_proxy_ && it->second->options().ipv6_enabled &&
       !it->second->options().find_ipv6_routes_legacy) ||
      it->second->options().fwd_multicast) {
    DeviceMessage msg;
    msg.set_dev_ifname(it->second->ifname());
    msg.set_teardown(true);
    IpHelperMessage ipm;
    *ipm.mutable_device_message() = msg;
    if (nd_proxy_ && it->second->options().ipv6_enabled &&
        !it->second->options().find_ipv6_routes_legacy)
      nd_proxy_->SendMessage(ipm);
    if (it->second->options().fwd_multicast)
      mcast_proxy_->SendMessage(ipm);
  }

  for (auto& h : rm_handlers_) {
    h.second.Run(it->second.get());
  }

  devices_.erase(it);
  return true;
}

Device* DeviceManager::FindByHostInterface(const std::string& ifname) const {
  // As long as the device list is small, this linear search is fine.
  for (auto& d : devices_) {
    if (d.second->config().host_ifname() == ifname)
      return d.second.get();
  }
  return nullptr;
}

Device* DeviceManager::FindByGuestInterface(const std::string& ifname) const {
  // As long as the device list is small, this linear search is fine.
  for (auto& d : devices_) {
    if (d.second->config().guest_ifname() == ifname)
      return d.second.get();
  }
  return nullptr;
}

bool DeviceManager::Exists(const std::string& name) const {
  return devices_.find(name) != devices_.end();
}

const std::string& DeviceManager::DefaultInterface() const {
  return default_ifname_;
}

void DeviceManager::LinkMsgHandler(const shill::RTNLMessage& msg) {
  if (!msg.HasAttribute(IFLA_IFNAME)) {
    LOG(ERROR) << "Link event message does not have IFLA_IFNAME";
    return;
  }

  // Only consider virtual interfaces that were created for guests; for now this
  // only includes those prefixed with 'arc'.
  shill::ByteString b(msg.GetAttribute(IFLA_IFNAME));
  std::string ifname(reinterpret_cast<const char*>(
      b.GetSubstring(0, IFNAMSIZ).GetConstData()));
  if (!IsArcDevice(ifname))
    return;

  bool link_up = msg.link_status().flags & IFF_UP;
  Device* device = FindByHostInterface(ifname);

  if (!device || !device->HostLinkUp(link_up))
    return;

  if (!link_up) {
    LOG(INFO) << ifname << " is now down";
    device->Disable();

    if ((nd_proxy_ && device->options().ipv6_enabled &&
         !device->options().find_ipv6_routes_legacy) ||
        device->options().fwd_multicast) {
      DeviceMessage msg;
      if (device->IsLegacyAndroid())
        msg.set_dev_ifname(default_ifname_);
      else
        msg.set_dev_ifname(device->ifname());
      msg.set_br_ifname(device->config().host_ifname());
      msg.set_teardown(true);
      IpHelperMessage ipm;
      *ipm.mutable_device_message() = msg;
      if (nd_proxy_ && device->options().ipv6_enabled &&
          !device->options().find_ipv6_routes_legacy)
        nd_proxy_->SendMessage(ipm);
      if (device->options().fwd_multicast)
        mcast_proxy_->SendMessage(ipm);
    }
    return;
  }

  // The link is now up.
  LOG(INFO) << ifname << " is now up";

  if (device->IsLegacyAndroid())
    device->Enable(default_ifname_);
  else if (!device->IsAndroid())
    device->Enable(device->config().guest_ifname());

  if ((nd_proxy_ != nullptr && device->options().ipv6_enabled &&
       !device->options().find_ipv6_routes_legacy) ||
      device->options().fwd_multicast) {
    DeviceMessage msg;
    if (device->IsLegacyAndroid())
      msg.set_dev_ifname(default_ifname_);
    else
      msg.set_dev_ifname(device->ifname());
    msg.set_guest_ip4addr(device->config().guest_ipv4_addr());
    msg.set_br_ifname(device->config().host_ifname());
    IpHelperMessage ipm;
    *ipm.mutable_device_message() = msg;
    if (nd_proxy_ != nullptr && device->options().ipv6_enabled &&
        !device->options().find_ipv6_routes_legacy)
      nd_proxy_->SendMessage(ipm);
    if (device->options().fwd_multicast)
      mcast_proxy_->SendMessage(ipm);
  }
}

std::unique_ptr<Device> DeviceManager::MakeDevice(
    const std::string& name) const {
  DCHECK(!name.empty());

  Device::Options opts;
  std::string host_ifname, guest_ifname;
  AddressManager::Guest guest = AddressManager::Guest::ARC;

  if (name == kAndroidLegacyDevice || name == kAndroidVmDevice) {
    if (name == kAndroidVmDevice)
      guest = AddressManager::Guest::VM_ARC;

    host_ifname = "arcbr0";
    guest_ifname = "arc0";
    opts.ipv6_enabled = true;
    opts.find_ipv6_routes_legacy = kFindIpv6RoutesLegacy;
    opts.fwd_multicast = true;
  } else {
    if (name == kAndroidDevice) {
      host_ifname = "arcbr0";
      opts.fwd_multicast = false;
    } else {
      guest = AddressManager::Guest::ARC_NET;
      host_ifname = base::StringPrintf("arc_%s", name.c_str());
      opts.fwd_multicast = IsMulticastInterface(name);
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
    // TODO(crbug/726815) Also enable |ipv6_enabled| for cellular networks
    // once IPv6 is enabled on cellular networks in shill.
    opts.ipv6_enabled =
        IsEthernetInterface(guest_ifname) || IsWifiInterface(guest_ifname);
    opts.find_ipv6_routes_legacy = kFindIpv6RoutesLegacy;
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

  return std::make_unique<Device>(name, std::move(config), opts);
}

void DeviceManager::OnDefaultInterfaceChanged(const std::string& ifname) {
  if (ifname == default_ifname_)
    return;

  LOG(INFO) << "Default interface changed from [" << default_ifname_ << "] to ["
            << ifname << "]";

  // On ARC N, we only forward multicast packets between default interface and
  // the bridge interface "arcbr0".
  // ND proxy will not be started here as it will not be shipped to ARC N.
  Device* device = FindByHostInterface("arcbr0");
  if (device && device->IsLegacyAndroid() && device->IsFullyUp()) {
    IpHelperMessage ipm;

    // Stop multicast forwarder on the old default interface.
    if (default_ifname_ != "") {
      // TODO(jasongustaman): When more guests are introduced, teardown the
      // bridge instead.
      DeviceMessage stop_msg;
      stop_msg.set_dev_ifname(default_ifname_);
      stop_msg.set_teardown(true);
      *ipm.mutable_device_message() = stop_msg;
      mcast_proxy_->SendMessage(ipm);
      mcast_proxy_->SendMessage(ipm);
    }

    // Start multicast forwarder on the new default interface.
    if (ifname != "") {
      DeviceMessage start_msg;
      start_msg.set_dev_ifname(ifname);
      start_msg.set_guest_ip4addr(device->config().guest_ipv4_addr());
      start_msg.set_br_ifname(device->config().host_ifname());
      *ipm.mutable_device_message() = start_msg;
      mcast_proxy_->SendMessage(ipm);
    }
  }

  default_ifname_ = ifname;
  for (const auto& h : default_iface_handlers_) {
    h.second.Run(default_ifname_);
  }
}

void DeviceManager::OnDevicesChanged(const std::set<std::string>& devices) {
  std::vector<std::string> removed;
  for (const auto& d : devices_) {
    const std::string& name = d.first;
    if (name != kAndroidDevice && name != kAndroidLegacyDevice &&
        devices.find(name) == devices.end())
      removed.emplace_back(name);
  }

  for (const std::string& name : removed)
    Remove(name);

  for (const std::string& name : devices)
    Add(name);
}

void DeviceManager::OnIPv6AddressFound(Device* device) {
  for (const auto& h : ipv6_handlers_) {
    h.second.Run(device);
  }
}

}  // namespace arc_networkd
