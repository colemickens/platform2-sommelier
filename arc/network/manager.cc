// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/manager.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <brillo/minijail/minijail.h>

#include "arc/network/ipc.pb.h"

namespace {

const char kMdnsMcastAddress[] = "224.0.0.251";
const uint16_t kMdnsPort = 5353;
const char kSsdpMcastAddress[] = "239.255.255.250";
const uint16_t kSsdpPort = 1900;

const int kMaxRandomAddressTries = 3;

const char kUnprivilegedUser[] = "arc-networkd";
const uint64_t kManagerCapMask = CAP_TO_MASK(CAP_NET_RAW);

}  // namespace

namespace arc_networkd {

Manager::Manager(const Options& opt, std::unique_ptr<HelperProcess> ip_helper)
    : int_ifname_(opt.int_ifname),
      mdns_ipaddr_(opt.mdns_ipaddr),
      con_ifname_(opt.con_ifname) {
  ip_helper_ = std::move(ip_helper);
}

int Manager::OnInit() {
  VLOG(1) << "OnInit for " << *this;

  // Run with minimal privileges.
  brillo::Minijail* m = brillo::Minijail::GetInstance();
  struct minijail* jail = m->New();

  // Most of these return void, but DropRoot() can fail if the user/group
  // does not exist.
  CHECK(m->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser))
      << "Could not drop root privileges";
  m->UseCapabilities(jail, kManagerCapMask);
  m->Enter(jail);
  m->Destroy(jail);

  // Handle subprocess lifecycle.

  process_reaper_.Register(this);

  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, ip_helper_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 ip_helper_->pid())))
      << "Failed to watch child IpHelper process";

  // This needs to execute after DBusDaemon::OnInit() creates bus_.
  base::MessageLoopForIO::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Manager::InitialSetup, weak_factory_.GetWeakPtr()));

  return DBusDaemon::OnInit();
}

void Manager::InitialSetup() {
  shill_client_.reset(new ShillClient(std::move(bus_)));
  shill_client_->RegisterDefaultInterfaceChangedHandler(base::Bind(
      &Manager::OnDefaultInterfaceChanged, weak_factory_.GetWeakPtr()));
}

void Manager::OnDefaultInterfaceChanged(const std::string& ifname) {
  ClearArcIp();
  neighbor_finder_.reset();

  if (ifname.empty()) {
    LOG(INFO) << "Unbinding services from interface " << lan_ifname_;
    lan_ifname_.clear();
    mdns_forwarder_.reset();
    ssdp_forwarder_.reset();
    router_finder_.reset();
    DisableInbound();
  } else {
    LOG(INFO) << "Binding services to new default interface " << ifname;
    lan_ifname_ = ifname;
    mdns_forwarder_.reset(new MulticastForwarder());
    ssdp_forwarder_.reset(new MulticastForwarder());
    router_finder_.reset(new RouterFinder());

    mdns_forwarder_->Start(int_ifname_, ifname, mdns_ipaddr_, kMdnsMcastAddress,
                           kMdnsPort,
                           /* allow_stateless */ true);
    ssdp_forwarder_->Start(int_ifname_, ifname,
                           /* mdns_ipaddr */ "", kSsdpMcastAddress, kSsdpPort,
                           /* allow_stateless */ false);

    router_finder_->Start(
        ifname, base::Bind(&Manager::OnRouteFound, weak_factory_.GetWeakPtr()));
    EnableInbound(ifname);
  }
}

void Manager::OnRouteFound(const struct in6_addr& prefix,
                           int prefix_len,
                           const struct in6_addr& router) {
  if (prefix_len == 64) {
    LOG(INFO) << "Found IPv6 network on iface " << lan_ifname_
              << " route=" << prefix << "/" << prefix_len
              << ", gateway=" << router;

    memcpy(&random_address_, &prefix, sizeof(random_address_));
    random_address_prefix_len_ = prefix_len;
    random_address_tries_ = 0;

    ArcIpConfig::GenerateRandom(&random_address_, random_address_prefix_len_);

    neighbor_finder_.reset(new NeighborFinder());
    neighbor_finder_->Check(lan_ifname_, random_address_,
                            base::Bind(&Manager::OnNeighborCheckResult,
                                       weak_factory_.GetWeakPtr()));
  } else {
    LOG(INFO) << "No IPv6 connectivity available";
  }
}

void Manager::OnNeighborCheckResult(bool found) {
  if (found) {
    if (++random_address_tries_ >= kMaxRandomAddressTries) {
      LOG(WARNING) << "Too many IP collisions, giving up.";
      return;
    }

    struct in6_addr previous_address = random_address_;
    ArcIpConfig::GenerateRandom(&random_address_, random_address_prefix_len_);

    LOG(INFO) << "Detected IP collision for " << previous_address
              << ", retrying with new address " << random_address_;

    neighbor_finder_->Check(lan_ifname_, random_address_,
                            base::Bind(&Manager::OnNeighborCheckResult,
                                       weak_factory_.GetWeakPtr()));
  } else {
    struct in6_addr router;

    if (!ArcIpConfig::GetV6Address(int_ifname_, &router)) {
      LOG(ERROR) << "Error reading link local address for " << int_ifname_;
      return;
    }

    LOG(INFO) << "Setting IPv6 address " << random_address_
              << "/128, gateway=" << router;

    // Set up new ARC IPv6 address, NDP, and forwarding rules.
    IpHelperMessage outer_msg;
    SetArcIp* inner_msg = outer_msg.mutable_set_arc_ip();
    inner_msg->set_prefix(&random_address_, sizeof(struct in6_addr));
    inner_msg->set_prefix_len(128);
    inner_msg->set_router(&router, sizeof(struct in6_addr));
    inner_msg->set_lan_ifname(lan_ifname_);
    ip_helper_->SendMessage(outer_msg);
  }
}

void Manager::ClearArcIp() {
  IpHelperMessage msg;
  msg.set_clear_arc_ip(true);
  ip_helper_->SendMessage(msg);
}

void Manager::EnableInbound(const std::string& lan_ifname) {
  IpHelperMessage msg;
  msg.set_enable_inbound(lan_ifname);
  ip_helper_->SendMessage(msg);
}

void Manager::DisableInbound() {
  IpHelperMessage msg;
  msg.set_disable_inbound(true);
  ip_helper_->SendMessage(msg);
}

void Manager::OnShutdown(int* exit_code) {
  ClearArcIp();
  DisableInbound();
}

void Manager::OnSubprocessExited(pid_t pid, const siginfo_t& info) {
  LOG(ERROR) << "Subprocess " << pid << " exited unexpectedly";
  Quit();
}

std::ostream& operator<<(std::ostream& stream, const Manager& manager) {
  stream << "Manager "
         << "{ inbound ifname=" << manager.lan_ifname_
         << ", host ifname=" << manager.int_ifname_
         << ", guest ifname=" << manager.con_ifname_
         << ", mdns ip=" << manager.mdns_ipaddr_
         << ", ipv6 random address=" << manager.random_address_
         << "/" << manager.random_address_prefix_len_
         << ", random address tries=" << manager.random_address_tries_
         << " }";
  return stream;
}

}  // namespace arc_networkd
