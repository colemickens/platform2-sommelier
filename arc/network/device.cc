// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device.h"

#include <sys/types.h>

#include <map>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "arc/network/arc_ip_config.h"

namespace arc_networkd {

const char kAndroidDevice[] = "android";

namespace {

const char kIPv4AddrFmt[] = "100.115.92.%d";
const char kMacAddrFmt[] = "00:FF:AA:00:00:%x";
const int kMacAddrBase = 85;

const char kMdnsMcastAddress[] = "224.0.0.251";
const uint16_t kMdnsPort = 5353;
const char kSsdpMcastAddress[] = "239.255.255.250";
const uint16_t kSsdpPort = 1900;

const int kMaxRandomAddressTries = 3;

bool AssignAddr(const std::string& ifname, DeviceConfig* config) {
  // To make this easier for now, hardcode the interfaces to support;
  // maps device names to the subnet base address (used in kIPv4AddrFmt).
  // Note that the .4/30 subnet is skipped since it is in use elsewhere.
  // TODO(garrick): Generalize and support arbitrary interfaces.
  static const std::map<std::string, int> cur_ifaces = {{kAndroidDevice, 0},
                                                        {"eth0", 8},
                                                        {"wlan0", 12},
                                                        {"wwan0", 16},
                                                        {"rmnet0", 20}};
  const auto it = cur_ifaces.find(ifname);
  if (it == cur_ifaces.end())
    return false;

  const int addr_off = it->second;
  config->set_br_ipv4(base::StringPrintf(kIPv4AddrFmt, addr_off + 1));
  config->set_arc_ipv4(base::StringPrintf(kIPv4AddrFmt, addr_off + 2));
  config->set_mac_addr(
      base::StringPrintf(kMacAddrFmt, kMacAddrBase + addr_off));
  return true;
}

}  // namespace

Device::Device(const std::string& ifname,
               const DeviceConfig& config,
               const MessageSink& msg_sink)
    : ifname_(ifname), config_(config), msg_sink_(msg_sink) {
  if (msg_sink_.is_null())
    return;

  IpHelperMessage msg;
  msg.set_dev_ifname(ifname_);
  *msg.mutable_dev_config() = config_;
  msg_sink_.Run(msg);
}

Device::~Device() {
  if (msg_sink_.is_null())
    return;

  IpHelperMessage msg;
  msg.set_dev_ifname(ifname_);
  msg.set_teardown(true);
  msg_sink_.Run(msg);
}

// static
std::unique_ptr<Device> Device::ForInterface(const std::string& ifname,
                                             const MessageSink& msg_sink) {
  DeviceConfig config;
  if (!AssignAddr(ifname, &config))
    return nullptr;

  if (ifname == kAndroidDevice) {
    config.set_br_ifname("arcbr0");
    config.set_arc_ifname("arc0");
    config.set_find_ipv6_routes(true);
  } else {
    config.set_br_ifname(base::StringPrintf("arc_%s", ifname.c_str()));
    config.set_arc_ifname(ifname);
    config.set_find_ipv6_routes(false);
  }
  // TODO(garrick): Enable only for Ethernet and Wifi.
  config.set_fwd_multicast(true);
  return std::make_unique<Device>(ifname, config, msg_sink);
}

void Device::Enable(const std::string& ifname) {
  LOG(INFO) << "Enabling device " << ifname_;
  if (!ifname.empty()) {
    CHECK_EQ(ifname_, kAndroidDevice);
    LOG(INFO) << "Binding interface " << ifname << " to device " << ifname_;
    legacy_lan_ifname_ = ifname;
  } else {
    legacy_lan_ifname_ = ifname_;
  }

  LOG(INFO) << "Enabling services for " << ifname_;
  // Enable inbound traffic.
  if (!msg_sink_.is_null()) {
    IpHelperMessage msg;
    msg.set_dev_ifname(ifname_);
    msg.set_enable_inbound_ifname(legacy_lan_ifname_);
    msg_sink_.Run(msg);
  }

  // TODO(garrick): Revisit multicast forwarding when NAT rules are enabled
  // for other devices.
  if (config_.fwd_multicast()) {
    mdns_forwarder_.reset(new MulticastForwarder());
    ssdp_forwarder_.reset(new MulticastForwarder());
    mdns_forwarder_->Start(config_.br_ifname(), legacy_lan_ifname_,
                           config_.arc_ipv4(), kMdnsMcastAddress, kMdnsPort,
                           /* allow_stateless */ true);
    ssdp_forwarder_->Start(config_.br_ifname(), legacy_lan_ifname_,
                           /* arc_ipaddr */ "", kSsdpMcastAddress, kSsdpPort,
                           /* allow_stateless */ false);
  }

  if (config_.find_ipv6_routes()) {
    router_finder_.reset(new RouterFinder());
    router_finder_->Start(
        legacy_lan_ifname_,
        base::Bind(&Device::OnRouteFound, weak_factory_.GetWeakPtr()));
  }
}

void Device::Disable() {
  LOG(INFO) << "Disabling services for " << ifname_;

  neighbor_finder_.reset();
  router_finder_.reset();
  ssdp_forwarder_.reset();
  mdns_forwarder_.reset();
  legacy_lan_ifname_.clear();

  if (msg_sink_.is_null())
    return;

  // Clear IPv6 info, if necessary.
  if (config_.find_ipv6_routes()) {
    IpHelperMessage msg;
    msg.set_dev_ifname(ifname_);
    msg.set_clear_arc_ip(true);
    msg_sink_.Run(msg);
  }

  // Disable inbound traffic.
  {
    IpHelperMessage msg;
    msg.set_dev_ifname(ifname_);
    msg.set_disable_inbound(true);
    msg_sink_.Run(msg);
  }
}

void Device::OnRouteFound(const struct in6_addr& prefix,
                          int prefix_len,
                          const struct in6_addr& router) {
  if (prefix_len == 64) {
    LOG(INFO) << "Found IPv6 network on iface " << legacy_lan_ifname_
              << " route=" << prefix << "/" << prefix_len
              << ", gateway=" << router;

    memcpy(&random_address_, &prefix, sizeof(random_address_));
    random_address_prefix_len_ = prefix_len;
    random_address_tries_ = 0;

    ArcIpConfig::GenerateRandom(&random_address_, random_address_prefix_len_);

    neighbor_finder_.reset(new NeighborFinder());
    neighbor_finder_->Check(
        legacy_lan_ifname_, random_address_,
        base::Bind(&Device::OnNeighborCheckResult, weak_factory_.GetWeakPtr()));
  } else {
    LOG(INFO) << "No IPv6 connectivity available on " << legacy_lan_ifname_;
  }
}

void Device::OnNeighborCheckResult(bool found) {
  if (found) {
    if (++random_address_tries_ >= kMaxRandomAddressTries) {
      LOG(WARNING) << "Too many IP collisions, giving up.";
      return;
    }

    struct in6_addr previous_address = random_address_;
    ArcIpConfig::GenerateRandom(&random_address_, random_address_prefix_len_);

    LOG(INFO) << "Detected IP collision for " << previous_address
              << ", retrying with new address " << random_address_;

    neighbor_finder_->Check(
        legacy_lan_ifname_, random_address_,
        base::Bind(&Device::OnNeighborCheckResult, weak_factory_.GetWeakPtr()));
  } else {
    struct in6_addr router;

    if (!ArcIpConfig::GetV6Address(config_.br_ifname(), &router)) {
      LOG(ERROR) << "Error reading link local address for "
                 << config_.br_ifname();
      return;
    }

    LOG(INFO) << "Setting IPv6 address " << random_address_
              << "/128, gateway=" << router << " on " << legacy_lan_ifname_;

    // Set up new ARC IPv6 address, NDP, and forwarding rules.
    if (!msg_sink_.is_null()) {
      IpHelperMessage msg;
      msg.set_dev_ifname(ifname_);
      SetArcIp* setup_msg = msg.mutable_set_arc_ip();
      setup_msg->set_prefix(&random_address_, sizeof(struct in6_addr));
      setup_msg->set_prefix_len(128);
      setup_msg->set_router(&router, sizeof(struct in6_addr));
      setup_msg->set_lan_ifname(legacy_lan_ifname_);
      msg_sink_.Run(msg);
    }
  }
}

}  // namespace arc_networkd
