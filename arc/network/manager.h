// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MANAGER_H_
#define ARC_NETWORK_MANAGER_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <string>

#include <base/memory/weak_ptr.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/process_reaper.h>

#include "arc/network/arc_ip_config.h"
#include "arc/network/helper_process.h"
#include "arc/network/multicast_forwarder.h"
#include "arc/network/neighbor_finder.h"
#include "arc/network/options.h"
#include "arc/network/router_finder.h"
#include "arc/network/shill_client.h"

namespace arc_networkd {

// Main class that runs the mainloop and responds to LAN interface changes.
class Manager final : public brillo::DBusDaemon {
 public:
  explicit Manager(const Options& opt,
                   std::unique_ptr<HelperProcess> ip_helper);

 protected:
  int OnInit() override;

 private:
  // Called once, after the dbus connection is established.
  void InitialSetup();

  // Delete ARC IPv6 address (if any).
  void ClearArcIp();

  // Enable or Disable inbound connections on |lan_ifname| by manipulating
  // a DNAT rule matching unclaimed traffic on the interface.
  void EnableInbound(const std::string& lan_ifname);
  void DisableInbound();

  // Callback from ShillClient, invoked whenever the default network
  // interface changes or goes away.
  void OnDefaultInterfaceChanged(const std::string& ifname);

  // Callback from RouterFinder.  May be triggered multiple times, e.g.
  // if the route disappears or changes.
  void OnRouteFound(const struct in6_addr& prefix,
                    int prefix_len,
                    const struct in6_addr& router);

  // Callback from NeighborFinder to indicate whether an IPv6 address
  // collision was found or not found.
  void OnNeighborCheckResult(bool found);

  // Callback from ProcessReaper to notify Manager that one of the
  // subprocesses died.
  void OnSubprocessExited(pid_t pid, const siginfo_t& info);

  // Callback from Daemon to notify Manager that a signal was received and
  // the daemon should clean up in preparation to exit.
  void OnShutdown(int* exit_code) override;

  friend std::ostream& operator<<(std::ostream& stream, const Manager& manager);

  // Persistent objects.
  brillo::ProcessReaper process_reaper_;
  std::unique_ptr<ShillClient> shill_client_;
  std::unique_ptr<HelperProcess> ip_helper_;

  std::string int_ifname_;
  std::string mdns_ipaddr_;
  std::string lan_ifname_;
  std::string con_ifname_;

  struct in6_addr random_address_;
  int random_address_prefix_len_;
  int random_address_tries_;

  // These get nuked every time the connection changes.  Deletion of
  // the object immediately stops all callbacks and activity on the
  // old interface.
  std::unique_ptr<MulticastForwarder> mdns_forwarder_;
  std::unique_ptr<MulticastForwarder> ssdp_forwarder_;
  std::unique_ptr<RouterFinder> router_finder_;
  std::unique_ptr<NeighborFinder> neighbor_finder_;

  base::WeakPtrFactory<Manager> weak_factory_{this};
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MANAGER_H_
