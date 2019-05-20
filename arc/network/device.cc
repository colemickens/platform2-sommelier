// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <map>
#include <utility>

#include <base/bind.h>
#include <base/lazy_instance.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "arc/network/arc_ip_config.h"

namespace arc_networkd {

// These are used to identify which ARC++ data path should be used when setting
// up the Android device.
const char kAndroidDevice[] = "arc0";
const char kAndroidLegacyDevice[] = "android";

namespace {

constexpr uint32_t kMdnsMcastAddress = 0xfb0000e0;  // 224.0.0.251 (NBO)
constexpr uint16_t kMdnsPort = 5353;
constexpr uint32_t kSsdpMcastAddress = 0xfaffffef;  // 239.255.255.250 (NBO)
constexpr uint16_t kSsdpPort = 1900;

constexpr int kMaxRandomAddressTries = 3;

std::string MacAddressToString(const MacAddress& addr) {
  return base::StringPrintf("%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1],
                            addr[2], addr[3], addr[4], addr[5]);
}

std::string IPv4AddressToString(uint32_t addr) {
  char buf[INET_ADDRSTRLEN] = {0};
  struct in_addr ia;
  ia.s_addr = addr;
  return !inet_ntop(AF_INET, &ia, buf, sizeof(buf)) ? "" : buf;
}

}  // namespace

Device::Config::Config(const std::string& host_ifname,
                       const std::string& guest_ifname,
                       const MacAddress& guest_mac_addr,
                       std::unique_ptr<Subnet> ipv4_subnet,
                       std::unique_ptr<SubnetAddress> host_ipv4_addr,
                       std::unique_ptr<SubnetAddress> guest_ipv4_addr)
    : host_ifname_(host_ifname),
      guest_ifname_(guest_ifname),
      guest_mac_addr_(guest_mac_addr),
      ipv4_subnet_(std::move(ipv4_subnet)),
      host_ipv4_addr_(std::move(host_ipv4_addr)),
      guest_ipv4_addr_(std::move(guest_ipv4_addr)) {}

Device::Device(const std::string& ifname,
               std::unique_ptr<Device::Config> config,
               const Device::Options& options,
               const MessageSink& msg_sink)
    : ifname_(ifname),
      config_(std::move(config)),
      options_(options),
      msg_sink_(msg_sink) {
  DCHECK(config_);
  if (msg_sink_.is_null())
    return;

  IpHelperMessage msg;
  msg.set_dev_ifname(ifname_);
  auto* dev_config = msg.mutable_dev_config();
  FillProto(dev_config);
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

void Device::FillProto(DeviceConfig* msg) {
  msg->set_br_ifname(config_->host_ifname());
  msg->set_br_ipv4(IPv4AddressToString(config_->host_ipv4_addr()));
  msg->set_arc_ifname(config_->guest_ifname());
  msg->set_arc_ipv4(IPv4AddressToString(config_->guest_ipv4_addr()));
  msg->set_mac_addr(MacAddressToString(config_->guest_mac_addr()));

  msg->set_fwd_multicast(options_.fwd_multicast);
  msg->set_find_ipv6_routes(options_.find_ipv6_routes);
}

void Device::Enable(const std::string& ifname) {
  LOG(INFO) << "Enabling device " << ifname_;

  // If operating in legacy single network mode, enable inbound traffic to ARC
  // from the interface.
  // TODO(b/77293260) Also enable inbound traffic rules specific to the input
  // physical interface in multinetworking mode.
  if (ifname_ == kAndroidLegacyDevice) {
    LOG(INFO) << "Binding interface " << ifname << " to device " << ifname_;
    legacy_lan_ifname_ = ifname;

    if (!msg_sink_.is_null()) {
      IpHelperMessage msg;
      msg.set_dev_ifname(ifname_);
      msg.set_enable_inbound_ifname(legacy_lan_ifname_);
      msg_sink_.Run(msg);
    }
  }

  if (options_.fwd_multicast) {
    mdns_forwarder_.reset(new MulticastForwarder());
    mdns_forwarder_->Start(config_->host_ifname(), ifname,
                           config_->guest_ipv4_addr(), kMdnsMcastAddress,
                           kMdnsPort,
                           /* allow_stateless */ true);

    ssdp_forwarder_.reset(new MulticastForwarder());
    ssdp_forwarder_->Start(config_->host_ifname(), ifname, INADDR_ANY,
                           kSsdpMcastAddress, kSsdpPort,
                           /* allow_stateless */ false);
  }

  if (options_.find_ipv6_routes) {
    router_finder_.reset(new RouterFinder());
    router_finder_->Start(
        ifname, base::Bind(&Device::OnRouteFound, weak_factory_.GetWeakPtr()));
  }
}

void Device::Disable() {
  LOG(INFO) << "Disabling device " << ifname_;

  neighbor_finder_.reset();
  router_finder_.reset();
  ssdp_forwarder_.reset();
  mdns_forwarder_.reset();

  if (msg_sink_.is_null())
    return;

  // Clear IPv6 info, if necessary.
  if (options_.find_ipv6_routes) {
    IpHelperMessage msg;
    msg.set_dev_ifname(ifname_);
    msg.set_clear_arc_ip(true);
    msg_sink_.Run(msg);
  }

  // Disable inbound traffic.
  // TODO(b/77293260) Also disable inbound traffic rules in multinetworking
  // mode.
  if (!legacy_lan_ifname_.empty()) {
    LOG(INFO) << "Unbinding interface " << legacy_lan_ifname_ << " from device "
              << ifname_;
    legacy_lan_ifname_.clear();

    IpHelperMessage msg;
    msg.set_dev_ifname(ifname_);
    msg.set_disable_inbound(true);
    msg_sink_.Run(msg);
  }
}

void Device::OnRouteFound(const struct in6_addr& prefix,
                          int prefix_len,
                          const struct in6_addr& router) {
  const std::string& ifname =
      legacy_lan_ifname_.empty() ? ifname_ : legacy_lan_ifname_;

  if (prefix_len == 64) {
    LOG(INFO) << "Found IPv6 network on iface " << ifname << " route=" << prefix
              << "/" << prefix_len << ", gateway=" << router;

    memcpy(&random_address_, &prefix, sizeof(random_address_));
    random_address_prefix_len_ = prefix_len;
    random_address_tries_ = 0;

    ArcIpConfig::GenerateRandom(&random_address_, random_address_prefix_len_);

    neighbor_finder_.reset(new NeighborFinder());
    neighbor_finder_->Check(
        ifname, random_address_,
        base::Bind(&Device::OnNeighborCheckResult, weak_factory_.GetWeakPtr()));
  } else {
    LOG(INFO) << "No IPv6 connectivity available on " << ifname;
  }
}

void Device::OnNeighborCheckResult(bool found) {
  const std::string& ifname =
      legacy_lan_ifname_.empty() ? ifname_ : legacy_lan_ifname_;

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
        ifname, random_address_,
        base::Bind(&Device::OnNeighborCheckResult, weak_factory_.GetWeakPtr()));
  } else {
    struct in6_addr router;

    if (!ArcIpConfig::GetV6Address(config_->host_ifname(), &router)) {
      LOG(ERROR) << "Error reading link local address for "
                 << config_->host_ifname();
      return;
    }

    LOG(INFO) << "Setting IPv6 address " << random_address_
              << "/128, gateway=" << router << " on " << ifname;

    // Set up new ARC IPv6 address, NDP, and forwarding rules.
    if (!msg_sink_.is_null()) {
      IpHelperMessage msg;
      msg.set_dev_ifname(ifname_);
      SetArcIp* setup_msg = msg.mutable_set_arc_ip();
      setup_msg->set_prefix(&random_address_, sizeof(struct in6_addr));
      setup_msg->set_prefix_len(128);
      setup_msg->set_router(&router, sizeof(struct in6_addr));
      setup_msg->set_lan_ifname(ifname);
      msg_sink_.Run(msg);
    }
  }
}

std::ostream& operator<<(std::ostream& stream, const Device& device) {
  stream << "{ ifname=" << device.ifname_;
  if (!device.legacy_lan_ifname_.empty())
    stream << ", legacy_lan_ifname=" << device.legacy_lan_ifname_;
  stream << ", bridge_ifname=" << device.config_->host_ifname()
         << ", guest_ifname=" << device.config_->guest_ifname()
         << ", guest_mac_addr="
         << MacAddressToString(device.config_->guest_mac_addr())
         // TODO(hugobenichi): add subnet prefix
         << ", bridge_ipv4_addr="
         << IPv4AddressToString(device.config_->host_ipv4_addr())
         << ", guest_ipv4_addr="
         << IPv4AddressToString(device.config_->guest_ipv4_addr())
         << ", fwd_multicast=" << device.options_.fwd_multicast
         << ", find_ipv6_routes=" << device.options_.find_ipv6_routes << '}';
  return stream;
}

}  // namespace arc_networkd
