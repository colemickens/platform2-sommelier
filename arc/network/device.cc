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

#include "arc/network/net_util.h"

namespace arc_networkd {

// These are used to identify which ARC++ data path should be used when setting
// up the Android device.
const char kAndroidDevice[] = "arc0";
const char kAndroidLegacyDevice[] = "android";

namespace {
constexpr uint32_t kMdnsMcastAddress = Ipv4Addr(224, 0, 0, 251);
constexpr uint16_t kMdnsPort = 5353;
constexpr uint32_t kSsdpMcastAddress = Ipv4Addr(239, 255, 255, 250);
constexpr uint16_t kSsdpPort = 1900;
constexpr int kMaxRandomAddressTries = 3;
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

void Device::IPv6Config::clear() {
  memset(&addr, 0, sizeof(struct in6_addr));
  memset(&router, 0, sizeof(struct in6_addr));
  prefix_len = 0;
  routing_table_id = -1;
  routing_table_attempts = 0;
  addr_attempts = 0;
  is_setup = false;
}

Device::Device(const std::string& ifname,
               std::unique_ptr<Device::Config> config,
               const Device::Options& options)
    : ifname_(ifname),
      config_(std::move(config)),
      options_(options),
      host_link_up_(false),
      guest_link_up_(false) {
  DCHECK(config_);
}

const std::string& Device::ifname() const {
  return ifname_;
}

Device::Config& Device::config() const {
  CHECK(config_);
  return *config_.get();
}

Device::IPv6Config& Device::ipv6_config() {
  return ipv6_config_;
}

const Device::Options& Device::options() const {
  return options_;
}

bool Device::IsAndroid() const {
  return ifname_ == kAndroidDevice;
}

bool Device::IsLegacyAndroid() const {
  return ifname_ == kAndroidLegacyDevice;
}

bool Device::LinkUp(const std::string& ifname, bool up) {
  bool* link_up =
      (ifname == config_->host_ifname())
          ? &host_link_up_
          : (ifname == config_->guest_ifname()) ? &guest_link_up_ : nullptr;
  if (!link_up) {
    LOG(DFATAL) << "Unknown interface: " << ifname;
    return false;
  }

  if (up == *link_up)
    return false;

  *link_up = up;
  return true;
}

void Device::Enable(const std::string& ifname) {
  if (!host_link_up_ || !guest_link_up_)
    return;

  if (options_.fwd_multicast) {
    if (!mdns_forwarder_) {
      LOG(INFO) << "Enabling mDNS forwarding for device " << ifname_;
      auto mdns_fwd = std::make_unique<MulticastForwarder>();
      if (mdns_fwd->Start(config_->host_ifname(), ifname,
                          config_->guest_ipv4_addr(), kMdnsMcastAddress,
                          kMdnsPort,
                          /* allow_stateless */ true)) {
        mdns_forwarder_ = std::move(mdns_fwd);
      } else {
        LOG(WARNING) << "mDNS forwarder could not be started on " << ifname_;
      }
    }

    if (!ssdp_forwarder_) {
      LOG(INFO) << "Enabling SSDP forwarding for device " << ifname_;
      auto ssdp_fwd = std::make_unique<MulticastForwarder>();
      if (ssdp_fwd->Start(config_->host_ifname(), ifname, htonl(INADDR_ANY),
                          kSsdpMcastAddress, kSsdpPort,
                          /* allow_stateless */ false)) {
        ssdp_forwarder_ = std::move(ssdp_fwd);
      } else {
        LOG(WARNING) << "SSDP forwarder could not be started on " << ifname_;
      }
    }
  }

  if (options_.find_ipv6_routes && !router_finder_) {
    LOG(INFO) << "Enabling IPV6 route finding for device " << ifname_
              << " on interface " << ifname;
    // In the case this is the Android device, |ifname| is the current default
    // interface and must be used.
    ipv6_config_.ifname = (IsAndroid() || IsLegacyAndroid()) ? ifname : ifname_;
    ipv6_config_.addr_attempts = 0;
    ipv6_config_.routing_table_attempts = 0;
    ipv6_config_.is_setup = false;
    router_finder_.reset(new RouterFinder());
    router_finder_->Start(
        ifname, base::Bind(&Device::OnRouteFound, weak_factory_.GetWeakPtr()));
  }
}

void Device::Disable() {
  if (neighbor_finder_ || router_finder_) {
    LOG(INFO) << "Disabling IPv6 route finding for device " << ifname_;
    neighbor_finder_.reset();
    router_finder_.reset();
  }
  if (ssdp_forwarder_) {
    LOG(INFO) << "Disabling SSDP forwarding for device " << ifname_;
    ssdp_forwarder_.reset();
  }
  if (mdns_forwarder_) {
    LOG(INFO) << "Disabling mDNS forwarding for device " << ifname_;
    mdns_forwarder_.reset();
  }

  if (!ipv6_down_handler_.is_null())
    ipv6_down_handler_.Run(this);

  ipv6_config_.clear();
}

void Device::RegisterIPv6SetupHandler(const DeviceHandler& handler) {
  ipv6_up_handler_ = handler;
}

void Device::RegisterIPv6TeardownHandler(const DeviceHandler& handler) {
  ipv6_down_handler_ = handler;
}

void Device::OnGuestStart(GuestMessage::GuestType guest) {
  host_link_up_ = false;
  guest_link_up_ = false;
}

void Device::OnGuestStop(GuestMessage::GuestType guest) {}

void Device::OnRouteFound(const struct in6_addr& prefix,
                          int prefix_len,
                          const struct in6_addr& router) {
  if (prefix_len != 64) {
    LOG(INFO) << "No IPv6 connectivity available on " << ipv6_config_.ifname
              << " - unsupported prefix length: " << prefix_len;
    return;
  }

  LOG(INFO) << "Found IPv6 network on iface " << ipv6_config_.ifname
            << " route=" << prefix << "/" << prefix_len
            << ", gateway=" << router;

  memcpy(&ipv6_config_.addr, &prefix, sizeof(ipv6_config_.addr));
  ipv6_config_.prefix_len = prefix_len;

  GenerateRandomIPv6Prefix(&ipv6_config_.addr, ipv6_config_.prefix_len);

  neighbor_finder_.reset(new NeighborFinder());
  neighbor_finder_->Check(
      ipv6_config_.ifname, ipv6_config_.addr,
      base::Bind(&Device::OnNeighborCheckResult, weak_factory_.GetWeakPtr()));
}

void Device::OnNeighborCheckResult(bool found) {
  if (found) {
    if (++ipv6_config_.addr_attempts >= kMaxRandomAddressTries) {
      LOG(WARNING) << "Too many IPv6 collisions, giving up.";
      return;
    }

    struct in6_addr previous_address = ipv6_config_.addr;
    GenerateRandomIPv6Prefix(&ipv6_config_.addr, ipv6_config_.prefix_len);

    LOG(INFO) << "Detected IP collision for " << previous_address
              << ", retrying with new address " << ipv6_config_.addr;

    neighbor_finder_->Check(
        ipv6_config_.ifname, ipv6_config_.addr,
        base::Bind(&Device::OnNeighborCheckResult, weak_factory_.GetWeakPtr()));
    return;
  }

  if (!FindFirstIPv6Address(config_->host_ifname(), &ipv6_config_.router)) {
    LOG(ERROR) << "Error reading link local address for "
               << config_->host_ifname();
    return;
  }

  if (!ipv6_up_handler_.is_null())
    ipv6_up_handler_.Run(this);
}

std::ostream& operator<<(std::ostream& stream, const Device& device) {
  stream << "{ ifname: " << device.ifname_
         << ", bridge_ifname: " << device.config_->host_ifname()
         << ", bridge_ipv4_addr: "
         << device.config_->host_ipv4_addr_->ToCidrString()
         << ", guest_ifname: " << device.config_->guest_ifname()
         << ", guest_ipv4_addr: "
         << device.config_->guest_ipv4_addr_->ToCidrString()
         << ", guest_mac_addr: "
         << MacAddressToString(device.config_->guest_mac_addr())
         << ", fwd_multicast: " << device.options_.fwd_multicast
         << ", find_ipv6_routes: " << device.options_.find_ipv6_routes << '}';
  return stream;
}

}  // namespace arc_networkd
