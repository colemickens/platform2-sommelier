// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_DEVICE_H_
#define ARC_NETWORK_DEVICE_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "arc/network/ipc.pb.h"
#include "arc/network/mac_address_generator.h"
#include "arc/network/multicast_forwarder.h"
#include "arc/network/neighbor_finder.h"
#include "arc/network/router_finder.h"
#include "arc/network/subnet.h"

namespace arc_networkd {

// Reserved name for the Android device.
extern const char kAndroidDevice[];
// Reserved name for the Android device for legacy single network configs.
extern const char kAndroidLegacyDevice[];

// Encapsulates a physical (e.g. eth0) or proxy (e.g. arc) network device and
// its configuration spec (interfaces, addresses) on the host and in the
// container. It manages additional services such as router detection, address
// assignment, and MDNS and SSDP forwarding. This class is the authoritative
// source for configuration events.
class Device {
 public:
  using MessageSink = base::Callback<void(const DeviceMessage&)>;

  class Config {
   public:
    Config(const std::string& host_ifname,
           const std::string& guest_ifname,
           const MacAddress& guest_mac_addr,
           std::unique_ptr<Subnet> ipv4_subnet,
           std::unique_ptr<SubnetAddress> host_ipv4_addr,
           std::unique_ptr<SubnetAddress> guest_ipv4_addr);
    ~Config() = default;

    std::string host_ifname() const { return host_ifname_; }
    std::string guest_ifname() const { return guest_ifname_; }
    MacAddress guest_mac_addr() const { return guest_mac_addr_; }
    uint32_t host_ipv4_addr() const { return host_ipv4_addr_->Address(); }
    uint32_t guest_ipv4_addr() const { return guest_ipv4_addr_->Address(); }

    friend std::ostream& operator<<(std::ostream& stream, const Device& device);

   private:
    std::string host_ifname_;
    std::string guest_ifname_;
    MacAddress guest_mac_addr_;
    std::unique_ptr<Subnet> ipv4_subnet_;
    std::unique_ptr<SubnetAddress> host_ipv4_addr_;
    std::unique_ptr<SubnetAddress> guest_ipv4_addr_;

    DISALLOW_COPY_AND_ASSIGN(Config);
  };

  struct Options {
    bool fwd_multicast;
    bool find_ipv6_routes;
  };

  Device(const std::string& ifname,
         std::unique_ptr<Config> config,
         const Options& options,
         const MessageSink& msg_sink);
  ~Device();

  void FillProto(DeviceConfig* msg) const;
  const std::string& ifname() const;
  Config& config() const;
  const Options& options() const;

  bool IsAndroid() const;
  bool IsLegacyAndroid() const;

  void Enable(const std::string& ifname);
  void Disable();

  void OnGuestStart(GuestMessage::GuestType guest);
  void OnGuestStop(GuestMessage::GuestType guest);

  // Updates the link status and returns whether the status was changed.
  // ifname must match either host_ifname or guest_ifname in the config.
  bool LinkUp(const std::string& ifname, bool up);

  friend std::ostream& operator<<(std::ostream& stream, const Device& device);

 private:
  // Callback from RouterFinder.  May be triggered multiple times, e.g.
  // if the route disappears or changes.
  void OnRouteFound(const struct in6_addr& prefix,
                    int prefix_len,
                    const struct in6_addr& router);

  // Callback from NeighborFinder to indicate whether an IPv6 address
  // collision was found or not found.
  void OnNeighborCheckResult(bool found);

  const std::string ifname_;
  std::unique_ptr<Config> config_;
  const Options options_;
  const MessageSink msg_sink_;

  // Only used for the legacy Android device; points to the interface currently
  // used by the container.
  std::string legacy_lan_ifname_;

  // Link status.
  // TODO(garrick): Scope by guest.
  bool host_link_up_;
  bool guest_link_up_;

  struct in6_addr random_address_;
  int random_address_prefix_len_;
  int random_address_tries_;

  std::unique_ptr<MulticastForwarder> mdns_forwarder_;
  std::unique_ptr<MulticastForwarder> ssdp_forwarder_;
  std::unique_ptr<RouterFinder> router_finder_;
  std::unique_ptr<NeighborFinder> neighbor_finder_;

  base::WeakPtrFactory<Device> weak_factory_{this};

  FRIEND_TEST(DeviceTest, DisableLegacyAndroidDeviceSendsTwoMessages);
  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_DEVICE_H_
