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
constexpr std::array<const char*, 2> kEthernetInterfacePrefixes{{"eth", "usb"}};
constexpr std::array<const char*, 2> kWifiInterfacePrefixes{{"wlan", "mlan"}};

bool IsArcDevice(const std::string& ifname) {
  return base::StartsWith(ifname, kArcDevicePrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsTerminaDevice(const std::string& ifname) {
  return base::StartsWith(ifname, kTerminaVmDevicePrefix,
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
                             Datapath* datapath,
                             HelperProcess* mcast_proxy,
                             HelperProcess* nd_proxy,
                             bool legacy_ipv6)
    : shill_client_(std::move(shill_client)),
      addr_mgr_(addr_mgr),
      datapath_(datapath),
      mcast_proxy_(mcast_proxy),
      nd_proxy_(nd_proxy),
      legacy_ipv6_(legacy_ipv6) {
  DCHECK(shill_client_);
  DCHECK(addr_mgr_);
  DCHECK(datapath_);
  CHECK(mcast_proxy_);
  CHECK(nd_proxy_);

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
  nd_proxy_->RegisterDeviceMessageHandler(base::Bind(
      &DeviceManager::OnDeviceMessageFromNDProxy, weak_factory_.GetWeakPtr()));
}

DeviceManager::~DeviceManager() {
  shill_client_->UnregisterDevicesChangedHandler();
  shill_client_->UnregisterDefaultInterfaceChangedHandler();
}

bool DeviceManager::IsMulticastInterface(const std::string& ifname) const {
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

void DeviceManager::UnregisterAllGuestHandlers(GuestMessage::GuestType guest) {
  add_handlers_.erase(guest);
  rm_handlers_.erase(guest);
  ipv6_handlers_.erase(guest);
  default_iface_handlers_.erase(guest);
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
  return AddWithContext(name, nullptr);
}

bool DeviceManager::AddWithContext(const std::string& name,
                                   std::unique_ptr<Device::Context> ctx) {
  if (name.empty() || Exists(name))
    return false;

  auto dev = MakeDevice(name);
  if (!dev)
    return false;

  if (ctx)
    dev->set_context(std::move(ctx));

  LOG(INFO) << "Adding device " << *dev;
  auto* device = dev.get();
  devices_.emplace(name, std::move(dev));

  if (device->options().ipv6_enabled &&
      device->options().find_ipv6_routes_legacy) {
    device->RegisterIPv6SetupHandler(base::Bind(
        &DeviceManager::OnIPv6AddressFound, weak_factory_.GetWeakPtr()));
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

  StopForwarding(*it->second);

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
  // This function now is only used for legacy IPv6 configuration for ARC
  // container, and should be removed after ndproxy enabled for all boards

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
    device->StopIPv6RoutingLegacy();
    return;
  }

  // The link is now up.
  LOG(INFO) << ifname << " is now up";

  if (device->UsesDefaultInterface())
    device->StartIPv6RoutingLegacy(default_ifname_);
  else if (!device->IsAndroid())
    device->StartIPv6RoutingLegacy(device->config().guest_ifname());
}

std::unique_ptr<Device> DeviceManager::MakeDevice(
    const std::string& name) const {
  DCHECK(!name.empty());

  Device::Options opts{
      .fwd_multicast = false,
      .ipv6_enabled = false,
      .find_ipv6_routes_legacy = legacy_ipv6_,
      .use_default_interface = false,
      .is_android = false,
      .is_sticky = false,
  };
  std::string host_ifname, guest_ifname;
  AddressManager::Guest guest = AddressManager::Guest::ARC;
  std::unique_ptr<Subnet> lxd_subnet;

  if (name == kAndroidLegacyDevice || name == kAndroidVmDevice) {
    host_ifname = "arcbr0";
    guest_ifname = "arc0";

    if (name == kAndroidVmDevice) {
      guest = AddressManager::Guest::VM_ARC;
      // (b/145644889) There are a couple things driving this device name:
      // 1. Until ArcNetworkService is running in ARCVM, arcbr0 cannot be resued
      // since the IPv4 addresses are different.
      // 2. Because of crbug/1008686, arcbr0 cannot be destroyed and recreated.
      // 3. Because shill only treats arcbr0 and devices prefixed with arc_ as
      // special, arc_br1 has to be used instead of arcbr1.
      // TODO(garrick): When either b/123431422 or the above crbug is fixed,
      // this can be removed.
      host_ifname = "arc_br1";
      guest_ifname = "arc1";
      opts.find_ipv6_routes_legacy = false;
    }

    opts.ipv6_enabled = true;
    opts.fwd_multicast = true;
    opts.use_default_interface = true;
    opts.is_android = true;
    opts.is_sticky = true;
  } else if (IsTerminaDevice(name)) {
    opts.ipv6_enabled = true;
    opts.find_ipv6_routes_legacy = false;
    opts.fwd_multicast = true;
    opts.use_default_interface = true;
    opts.is_sticky = true;
    guest = AddressManager::Guest::VM_TERMINA;
    lxd_subnet =
        addr_mgr_->AllocateIPv4Subnet(AddressManager::Guest::CONTAINER);
    if (!lxd_subnet) {
      LOG(ERROR) << "lxd subnet already in use or unavailable."
                 << " Cannot make device: " << name;
      return nullptr;
    }
  } else {
    if (name == kAndroidDevice) {
      host_ifname = "arcbr0";
      opts.is_android = true;
      opts.is_sticky = true;
    } else {
      guest = AddressManager::Guest::ARC_NET;
      host_ifname = base::StringPrintf("arc_%s", name.c_str());
      opts.fwd_multicast = IsMulticastInterface(name);
    }
    guest_ifname = name;
    // TODO(crbug/726815) Also enable |ipv6_enabled| for cellular networks
    // once IPv6 is enabled on cellular networks in shill.
    opts.ipv6_enabled =
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
      std::move(guest_ipv4_addr), std::move(lxd_subnet));

  // ARC guest is used for any variant (N, P, VM).
  return std::make_unique<Device>(name, std::move(config), opts,
                                  guest == AddressManager::Guest::VM_TERMINA
                                      ? GuestMessage::TERMINA_VM
                                      : GuestMessage::ARC);
}

void DeviceManager::OnDefaultInterfaceChanged(const std::string& ifname) {
  if (ifname == default_ifname_)
    return;

  LOG(INFO) << "Default interface changed from [" << default_ifname_ << "] to ["
            << ifname << "]";

  for (const auto& d : devices_) {
    if (d.second->UsesDefaultInterface() && d.second->IsFullyUp())
      StopForwarding(*d.second);
  }

  default_ifname_ = ifname;

  for (const auto& d : devices_) {
    if (d.second->UsesDefaultInterface() && d.second->IsFullyUp())
      StartForwarding(*d.second);
  }

  for (const auto& h : default_iface_handlers_) {
    h.second.Run(default_ifname_);
  }
}

void DeviceManager::StartForwarding(const Device& device) {
  std::string ifname_physical =
      device.UsesDefaultInterface() ? default_ifname_ : device.ifname();
  if (ifname_physical.empty())
    return;
  std::string ifname_virtual = device.HostInterfaceName();

  LOG(INFO) << "Start forwarding from " << ifname_physical << " to "
            << ifname_virtual;

  DeviceMessage msg;
  msg.set_dev_ifname(ifname_physical);
  msg.set_guest_ip4addr(device.config().guest_ipv4_addr());
  msg.set_br_ifname(ifname_virtual);

  IpHelperMessage ipm;
  *ipm.mutable_device_message() = msg;

  if (device.options().ipv6_enabled &&
      !device.options().find_ipv6_routes_legacy) {
    if (!datapath_->AddIPv6Forwarding(ifname_physical, ifname_virtual)) {
      LOG(ERROR) << "Failed to setup iptables forwarding rule for IPv6 from "
                 << ifname_physical << " to " << ifname_virtual;
    }
    if (!datapath_->MaskInterfaceFlags(ifname_physical, IFF_ALLMULTI)) {
      LOG(WARNING) << "Failed to setup all multicast mode for interface "
                   << ifname_physical;
    }
    if (!datapath_->MaskInterfaceFlags(ifname_virtual, IFF_ALLMULTI)) {
      LOG(WARNING) << "Failed to setup all multicast mode for interface "
                   << ifname_virtual;
    }
    nd_proxy_->SendMessage(ipm);
  }
  if (device.options().fwd_multicast)
    mcast_proxy_->SendMessage(ipm);
}

void DeviceManager::StopForwarding(const Device& device) {
  std::string ifname_physical =
      device.UsesDefaultInterface() ? default_ifname_ : device.ifname();
  if (ifname_physical.empty())
    return;
  std::string ifname_virtual = device.HostInterfaceName();
  LOG(INFO) << "Stop forwarding from " << ifname_physical << " to "
            << ifname_virtual;

  DeviceMessage msg;
  msg.set_dev_ifname(ifname_physical);
  msg.set_teardown(true);

  IpHelperMessage ipm;
  *ipm.mutable_device_message() = msg;

  if (device.options().ipv6_enabled &&
      !device.options().find_ipv6_routes_legacy) {
    datapath_->RemoveIPv6Forwarding(ifname_physical, ifname_virtual);
    nd_proxy_->SendMessage(ipm);
  }
  if (device.options().fwd_multicast)
    mcast_proxy_->SendMessage(ipm);
}

void DeviceManager::OnDevicesChanged(const std::set<std::string>& devices) {
  std::vector<std::string> removed;
  for (const auto& d : devices_) {
    const std::string& name = d.first;
    if (!d.second->options().is_sticky && devices.find(name) == devices.end())
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
