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

#include <base/memory/weak_ptr.h>

#include "arc/network/ipc.pb.h"
#include "arc/network/multicast_forwarder.h"
#include "arc/network/neighbor_finder.h"
#include "arc/network/router_finder.h"

namespace arc_networkd {

extern const char kAndroidDevice[];

// Encapsulates a physical (e.g. eth0) or proxy (e.g. arc) network device and
// its configuration spec (interfaces, addresses) on the host and in the
// container. It manages additional services such as router detection, address
// assignment, and MDNS and SSDP forwarding. This class is the authoritative
// source for configuration events.
class Device {
 public:
  Device(const std::string& ifname, const DeviceConfig& config);
  virtual ~Device();

  // Returns |nullptr| if the device configuration could not be generated
  // for the interface.
  static std::unique_ptr<Device> ForInterface(const std::string& ifname);

  // Callback used to deliver device configuration events.
  void RegisterMessageSink(
      const base::Callback<void(const IpHelperMessage&)>& callback);

  // |ifname| should always be empty for devices that represent physical host
  // interfaces. For others, like the ARC device 'android' that represents the
  // arcbr0 and arc0 interfaces, this function enables traffic to flow from a
  // real interfaces (e.g. eth0) to the container.
  void Enable(const std::string& ifname);
  void Disable();

  // Sends a message to the helper process announcing the discovery of a new
  // device.
  void Announce();

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
  const DeviceConfig config_;
  // Only used for "android" device; points to the interface currently
  // used by the container.
  std::string legacy_lan_ifname_;

  struct in6_addr random_address_;
  int random_address_prefix_len_;
  int random_address_tries_;

  std::unique_ptr<MulticastForwarder> mdns_forwarder_;
  std::unique_ptr<MulticastForwarder> ssdp_forwarder_;
  std::unique_ptr<RouterFinder> router_finder_;
  std::unique_ptr<NeighborFinder> neighbor_finder_;

  base::Callback<void(const IpHelperMessage&)> msg_sink_;
  base::WeakPtrFactory<Device> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_DEVICE_H_
