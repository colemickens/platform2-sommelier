// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_ip_config.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <random>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "arc/network/scoped_ns.h"

namespace {

constexpr int kInvalidNs = 0;
constexpr int kInvalidTableId = -1;
constexpr int kMaxTableRetries = 10;  // Based on 1 second delay.
constexpr base::TimeDelta kTableRetryDelay = base::TimeDelta::FromSeconds(1);
constexpr char kDefaultNetmask[] = "255.255.255.252";

const struct in6_addr* ExtractAddr6(const std::string& in) {
  if (in.size() != sizeof(struct in6_addr)) {
    LOG(DFATAL) << "Size mismatch";
    return nullptr;
  }
  return reinterpret_cast<const struct in6_addr*>(in.data());
}

bool ValidateIfname(const std::string& ifname) {
  if (ifname.size() >= IFNAMSIZ) {
    return false;
  }
  for (const char& c : ifname) {
    if (!isalnum(c) && c != '_') {
      return false;
    }
  }
  return true;
}

// Returns for given interface name the host name of a ARC veth pair.
std::string ArcVethHostName(std::string ifname) {
  return "veth_" + ifname;
}

// Returns for given interface name the peer name of a ARC veth pair.
std::string ArcVethPeerName(std::string ifname) {
  return "peer_" + ifname;
}
}  // namespace

namespace arc_networkd {

ArcIpConfig::ArcIpConfig(const std::string& ifname, const DeviceConfig& config)
    : ArcIpConfig(ifname, config, std::make_unique<MinijailedProcessRunner>()) {
}

ArcIpConfig::ArcIpConfig(
    const std::string& ifname,
    const DeviceConfig& config,
    std::unique_ptr<MinijailedProcessRunner> process_runner)
    : ifname_(ifname),
      config_(config),
      con_netns_(kInvalidNs),
      routing_table_id_(kInvalidTableId),
      routing_table_attempts_(0),
      if_up_(false),
      ipv6_configured_(false),
      inbound_configured_(false),
      ipv6_dev_ifname_(ifname),
      process_runner_(std::move(process_runner)) {
  Setup();
}

ArcIpConfig::~ArcIpConfig() {
  Teardown();
}

void ArcIpConfig::Setup() {
  LOG(INFO) << "Setting up " << ifname_ << " bridge: " << config_.br_ifname()
            << " guest_iface: " << config_.arc_ifname();

  // Configure the persistent Chrome OS bridge interface with static IP.
  process_runner_->Run({kBrctlPath, "addbr", config_.br_ifname()});
  process_runner_->Run({kIfConfigPath, config_.br_ifname(), config_.br_ipv4(),
                        "netmask", kDefaultNetmask, "up"});
  // See nat.conf in chromeos-nat-init for the rest of the NAT setup rules.
  process_runner_->Run({kIpTablesPath, "-t", "mangle", "-A", "PREROUTING", "-i",
                        config_.br_ifname(), "-j", "MARK", "--set-mark", "1",
                        "-w"});

  // The legacy Android device is configured to support container traffic
  // coming from the default (shill) interface but this isn't necessary in
  // the mutli-net case where this interface is really just preserving the
  // known address mapping for the arc0 interface.
  if (ifname_ == kAndroidLegacyDevice) {
    // Sanity check.
    CHECK_EQ("arcbr0", config_.br_ifname());
    CHECK_EQ("arc0", config_.arc_ifname());

    // Forward "unclaimed" packets to Android to allow inbound connections
    // from devices on the LAN.
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-N", "dnat_arc", "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "dnat_arc", "-j",
                          "DNAT", "--to-destination", config_.arc_ipv4(),
                          "-w"});

    // This chain is dynamically updated whenever the default interface
    // changes.
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-N", "try_arc", "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-m",
                          "socket", "--nowildcard", "-j", "ACCEPT", "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-p",
                          "tcp", "-j", "try_arc", "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-p",
                          "udp", "-j", "try_arc", "-w"});

    process_runner_->Run({kIpTablesPath, "-t", "filter", "-A", "FORWARD", "-o",
                          config_.br_ifname(), "-j", "ACCEPT", "-w"});
  } else if (ifname_ != kAndroidDevice) {
    // Direct ingress IP traffic to existing sockets.
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-i",
                          ifname_, "-m", "socket", "--nowildcard", "-j",
                          "ACCEPT", "-w"});
    // Direct ingress TCP & UDP traffic to ARC interface for new connections.
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-i",
                          ifname_, "-p", "tcp", "-j", "DNAT",
                          "--to-destination", config_.arc_ipv4(), "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-i",
                          ifname_, "-p", "udp", "-j", "DNAT",
                          "--to-destination", config_.arc_ipv4(), "-w"});
    // TODO(garrick): Verify this is still needed.
    process_runner_->Run({kIpTablesPath, "-t", "filter", "-A", "FORWARD", "-o",
                          config_.br_ifname(), "-j", "ACCEPT", "-w"});
  }
}

void ArcIpConfig::Teardown() {
  LOG(INFO) << "Tearing down " << ifname_ << " bridge: " << config_.br_ifname()
            << " guest_iface: " << config_.arc_ifname();
  Clear();

  if (ifname_ == kAndroidLegacyDevice) {
    DisableInbound();

    process_runner_->Run({kIpTablesPath, "-t", "filter", "-D", "FORWARD", "-o",
                          config_.br_ifname(), "-j", "ACCEPT", "-w"});

    process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-p",
                          "udp", "-j", "try_arc", "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-p",
                          "tcp", "-j", "try_arc", "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-m",
                          "socket", "--nowildcard", "-j", "ACCEPT", "-w"});

    process_runner_->Run({kIpTablesPath, "-t", "nat", "-F", "try_arc", "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-X", "try_arc", "-w"});

    process_runner_->Run({kIpTablesPath, "-t", "nat", "-F", "dnat_arc", "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-X", "dnat_arc", "-w"});
  } else if (ifname_ != kAndroidDevice) {
    process_runner_->Run({kIpTablesPath, "-t", "filter", "-D", "FORWARD", "-o",
                          config_.br_ifname(), "-j", "ACCEPT", "-w"});

    process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-i",
                          ifname_, "-p", "udp", "-j", "DNAT",
                          "--to-destination", config_.arc_ipv4(), "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-i",
                          ifname_, "-p", "tcp", "-j", "DNAT",
                          "--to-destination", config_.arc_ipv4(), "-w"});
    process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-i",
                          ifname_, "-m", "socket", "--nowildcard", "-j",
                          "ACCEPT", "-w"});

    process_runner_->Run({kIpPath, "link", "delete", ArcVethHostName(ifname_)},
                         true /* log_failures */);
  }

  process_runner_->Run({kIpTablesPath, "-t", "mangle", "-D", "PREROUTING", "-i",
                        config_.br_ifname(), "-j", "MARK", "--set-mark", "1",
                        "-w"});

  process_runner_->Run({kIfConfigPath, config_.br_ifname(), "down"});
  process_runner_->Run({kBrctlPath, "delbr", config_.br_ifname()});
}

bool ArcIpConfig::Init(pid_t con_netns) {
  if (!con_netns) {
    LOG(INFO) << "Uninitializing " << ifname_
              << " bridge: " << config_.br_ifname()
              << " guest_iface: " << config_.arc_ifname();
    ContainerReady(false);
    con_netns_ = 0;
    return true;
  }

  con_netns_ = con_netns;

  LOG(INFO) << "Initializing " << ifname_ << " bridge: " << config_.br_ifname()
            << " guest_iface: " << config_.arc_ifname() << " for container pid "
            << con_netns_;

  const std::string pid = base::IntToString(con_netns_);
  const std::string veth = ArcVethHostName(ifname_);
  const std::string peer = ArcVethPeerName(ifname_);
  process_runner_->Run({kIpPath, "link", "delete", veth},
                       false /* log_failures */);
  process_runner_->Run(
      {kIpPath, "link", "add", veth, "type", "veth", "peer", "name", peer});
  process_runner_->Run({kIfConfigPath, veth, "up"});
  process_runner_->Run({kIpPath, "link", "set", "dev", peer, "addr",
                        config_.mac_addr(), "down"});
  process_runner_->Run({kBrctlPath, "addif", config_.br_ifname(), veth});

  // Container ns needs to be ready here. For now this is gated by the wait
  // loop the conf file.
  // TODO(garrick): Run this in response to the RTNETLINK (NEWNSID) event:
  // https://elixir.bootlin.com/linux/v4.14/source/net/core/net_namespace.c#L234

  process_runner_->Run({kIpPath, "link", "set", peer, "netns", pid});
  process_runner_->AddInterfaceToContainer(peer, config_.arc_ifname(),
                                           config_.arc_ipv4(), kDefaultNetmask,
                                           config_.fwd_multicast(), pid);

  // Signal the container that the network device is ready.
  // This is only applicable for arc0.
  if (ifname_ == kAndroidDevice || ifname_ == kAndroidLegacyDevice) {
    process_runner_->WriteSentinelToContainer(pid);
  }
  return true;
}

void ArcIpConfig::ContainerReady(bool ready) {
  if (!if_up_ && ready) {
    LOG(INFO) << config_.arc_ifname() << " is now up.";
  } else if (if_up_ && !ready) {
    LOG(INFO) << config_.arc_ifname() << " is now down.";
  }
  if_up_ = ready;
  if (if_up_) {
    if (!pending_inbound_ifname_.empty()) {
      std::string ifname = std::move(pending_inbound_ifname_);
      EnableInbound(ifname);
    }
    if (pending_ipv6_) {
      SetArcIp arc_ip = *pending_ipv6_.get();
      pending_ipv6_.reset();
      Set(arc_ip);
    }
  }
}

void ArcIpConfig::AssignTableIdForArcInterface() {
  if (routing_table_id_ != kInvalidTableId)
    return;

  base::FilePath ifindex_path(
      base::StringPrintf("/proc/%d/root/sys/class/net/%s/ifindex", con_netns_,
                         config_.arc_ifname().c_str()));
  std::string contents;
  if (!base::ReadFileToString(ifindex_path, &contents)) {
    PLOG(WARNING) << "Could not read " << ifindex_path.value();
    return;
  }
  base::TrimWhitespaceASCII(contents, base::TRIM_TRAILING, &contents);
  int table_id = kInvalidTableId;
  if (!base::StringToInt(contents, &table_id)) {
    LOG(ERROR) << "Could not parse ifindex from " << ifindex_path.value()
               << ": " << contents;
    return;
  }

  // Android adds a constant to the interface index to derive the table id.
  // This is defined in system/netd/server/RouteController.h
  constexpr int kRouteControllerRouteTableOffsetFromIndex = 1000;
  table_id += kRouteControllerRouteTableOffsetFromIndex;

  LOG(INFO) << "Found table id " << table_id << " for container iface "
            << config_.arc_ifname();

  routing_table_id_ = table_id;
}

// static
void ArcIpConfig::GenerateRandom(struct in6_addr* prefix, int prefix_len) {
  std::mt19937 rng;
  rng.seed(std::random_device()());
  std::uniform_int_distribution<std::mt19937::result_type> randbyte(0, 255);

  // TODO(cernekee): handle different prefix lengths
  CHECK_EQ(prefix_len, 64);

  for (int i = 8; i < 16; i++)
    prefix->s6_addr[i] = randbyte(rng);

  // Set the universal/local flag, similar to a RFC 4941 address.
  prefix->s6_addr[8] |= 0x40;
}

// static
bool ArcIpConfig::GetV6Address(const std::string& ifname,
                               struct in6_addr* address) {
  struct ifaddrs* ifap;
  struct ifaddrs* p;
  bool found = false;

  // Iterate through the linked list of all interface addresses to find
  // the first IPv6 address for |ifname|.
  if (getifaddrs(&ifap) < 0)
    return false;

  for (p = ifap; p; p = p->ifa_next) {
    if (p->ifa_name != ifname || p->ifa_addr->sa_family != AF_INET6) {
      continue;
    }

    if (address) {
      struct sockaddr_in6* sa =
          reinterpret_cast<struct sockaddr_in6*>(p->ifa_addr);
      memcpy(address, &sa->sin6_addr, sizeof(*address));
    }
    found = true;
    break;
  }

  freeifaddrs(ifap);
  return found;
}

void ArcIpConfig::Set(const SetArcIp& arc_ip) {
  Clear();

  if (!if_up_) {
    LOG(INFO) << "Setting IPv6 for " << config_.arc_ifname()
              << " pending container interface up.";
    pending_ipv6_.reset(new SetArcIp);
    *pending_ipv6_.get() = arc_ip;
    return;
  }

  // If this device config has not yet been initialized then just return.
  // This allows for the IPv6 settings to arrive beforehand but also
  // prevents the retry loop below from executing if the device was shut
  // down before completing.
  if (con_netns_ == kInvalidNs)
    return;

  if (arc_ip.prefix_len() == 0 || arc_ip.prefix_len() > 128) {
    LOG(DFATAL) << "Invalid prefix len " << arc_ip.prefix_len();
    return;
  }
  if (!ValidateIfname(arc_ip.lan_ifname())) {
    LOG(DFATAL) << "Invalid inbound iface name " << arc_ip.lan_ifname();
    return;
  }

  const struct in6_addr* address = ExtractAddr6(arc_ip.prefix());
  if (!address)
    return;

  const struct in6_addr* router_addr = ExtractAddr6(arc_ip.router());
  if (!router_addr)
    return;

  // If we cannot find the routing table id yet, it could either be a race with
  // Android setting it up, or something legitimately bad happened; so be sure
  // to try several times before giving up for good.
  AssignTableIdForArcInterface();
  if (routing_table_id_ == kInvalidTableId) {
    if (routing_table_attempts_++ < kMaxTableRetries) {
      LOG(INFO) << "Could not look up routing table ID for container interface "
                << config_.arc_ifname() << " - trying again...";
      base::MessageLoop::current()->task_runner()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ArcIpConfig::Set, weak_factory_.GetWeakPtr(), arc_ip),
          kTableRetryDelay);
    } else {
      LOG(DFATAL)
          << "Could not look up routing table ID for container interface "
          << config_.arc_ifname();
    }
    return;
  }

  char buf[INET6_ADDRSTRLEN];
  if (!inet_ntop(AF_INET6, address, buf, sizeof(buf))) {
    LOG(DFATAL) << "Invalid address: " << address;
    return;
  }
  ipv6_address_ = buf;
  ipv6_address_full_ = ipv6_address_;
  ipv6_address_full_.append(
      "/" + std::to_string(static_cast<int>(arc_ip.prefix_len())));

  if (!inet_ntop(AF_INET6, router_addr, buf, sizeof(buf))) {
    LOG(DFATAL) << "Invalid router address: " << router_addr;
    return;
  }
  ipv6_router_ = buf;

  // This is needed to support the single network legacy case.
  // If this isn't the legacy device, then ensure the interface is the same.
  if (ifname_ != kAndroidLegacyDevice && ifname_ != arc_ip.lan_ifname()) {
    LOG(DFATAL) << "Mismatched interfaces " << ifname_ << " vs "
                << arc_ip.lan_ifname();
    return;
  }
  ipv6_dev_ifname_ = arc_ip.lan_ifname();

  LOG(INFO) << "Setting " << *this;
  {
    ScopedNS ns(con_netns_);
    if (ns.IsValid()) {
      // These can fail if the interface disappears (e.g. hot-unplug).
      // If that happens, the error will be logged, because sometimes it
      // might help in debugging a real issue.

      process_runner_->Run({kIpPath, "-6", "addr", "add", ipv6_address_full_,
                            "dev", config_.arc_ifname()});

      process_runner_->Run({kIpPath, "-6", "route", "add", ipv6_router_, "dev",
                            config_.arc_ifname(), "table",
                            std::to_string(routing_table_id_)});

      process_runner_->Run({kIpPath, "-6", "route", "add", "default", "via",
                            ipv6_router_, "dev", config_.arc_ifname(), "table",
                            std::to_string(routing_table_id_)});
    }
  }

  process_runner_->Run({kIpPath, "-6", "route", "add", ipv6_address_full_,
                        "dev", config_.br_ifname()});

  process_runner_->Run({kIpPath, "-6", "neigh", "add", "proxy", ipv6_address_,
                        "dev", ipv6_dev_ifname_});

  // These should never fail.
  if (process_runner_->Run({kIp6TablesPath, "-A", "FORWARD", "-i",
                            ipv6_dev_ifname_, "-o", config_.br_ifname(), "-j",
                            "ACCEPT", "-w"}) != 0) {
    LOG(DFATAL) << "Could not update ip6tables";
    return;
  }

  if (process_runner_->Run({kIp6TablesPath, "-A", "FORWARD", "-i",
                            config_.br_ifname(), "-o", ipv6_dev_ifname_, "-j",
                            "ACCEPT", "-w"}) != 0) {
    LOG(DFATAL) << "Could not update ip6tables";
    return;
  }

  ipv6_configured_ = true;
}

void ArcIpConfig::Clear() {
  if (pending_ipv6_) {
    LOG(INFO) << "Clearing pending IPv6 settings for " << config_.arc_ifname();
    pending_ipv6_.reset();
  }

  int routing_tid = routing_table_id_;
  routing_table_id_ = kInvalidTableId;
  routing_table_attempts_ = 0;
  if (!ipv6_configured_)
    return;

  LOG(INFO) << "Clearing " << *this;

  // These should never fail.
  if (process_runner_->Run({kIp6TablesPath, "-D", "FORWARD", "-i",
                            config_.br_ifname(), "-o", ipv6_dev_ifname_, "-j",
                            "ACCEPT", "-w"}) != 0) {
    LOG(DFATAL) << "Could not update ip6tables";
  }

  if (process_runner_->Run({kIp6TablesPath, "-D", "FORWARD", "-i",
                            ipv6_dev_ifname_, "-o", config_.br_ifname(), "-j",
                            "ACCEPT", "-w"}) != 0) {
    LOG(DFATAL) << "Could not update ip6tables";
  }

  // This often fails because the kernel removes the proxy entry
  // automatically.

  process_runner_->Run({kIpPath, "-6", "neigh", "del", "proxy", ipv6_address_,
                        "dev", ipv6_dev_ifname_},
                       false /* log_failures */);

  // This can fail if the interface disappears (e.g. hot-unplug).  Rare.

  process_runner_->Run({kIpPath, "-6", "route", "del", ipv6_address_full_,
                        "dev", config_.br_ifname()});

  {
    ScopedNS ns(con_netns_);
    if (ns.IsValid()) {
      process_runner_->Run({kIpPath, "-6", "route", "del", "default", "via",
                            ipv6_router_, "dev", config_.arc_ifname(), "table",
                            std::to_string(routing_tid)});

      process_runner_->Run({kIpPath, "-6", "route", "del", ipv6_router_, "dev",
                            config_.arc_ifname(), "table",
                            std::to_string(routing_tid)});

      // This often fails because ARC tries to delete the address on its own
      // when it is notified that the LAN is down.

      process_runner_->Run({kIpPath, "-6", "addr", "del", ipv6_address_full_,
                            "dev", config_.arc_ifname()},
                           false);
    }
  }

  ipv6_dev_ifname_.clear();
  ipv6_configured_ = false;
}

void ArcIpConfig::EnableInbound(const std::string& lan_ifname) {
  if (ifname_ != kAndroidLegacyDevice) {
    LOG(ERROR) << "Enabling inbound traffic on non-legacy device is unexpected "
                  "and not supported: "
               << ifname_;
    return;
  }

  if (!if_up_) {
    LOG(INFO) << "Enable inbound for " << ifname_ << " ["
              << config_.arc_ifname() << "]"
              << " on " << lan_ifname << " pending on container interface up.";
    pending_inbound_ifname_ = lan_ifname;
    return;
  }

  DisableInbound();

  LOG(INFO) << "Enabling inbound for " << ifname_ << " ["
            << config_.arc_ifname() << "]"
            << " on " << lan_ifname;

  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "try_arc", "-i",
                            lan_ifname, "-j", "dnat_arc", "-w"}) != 0) {
    LOG(DFATAL) << "Could not update iptables";
    return;
  }

  inbound_configured_ = true;
}

void ArcIpConfig::DisableInbound() {
  if (!pending_inbound_ifname_.empty()) {
    LOG(INFO) << "Clearing pending inbound request for " << ifname_ << " ["
              << config_.arc_ifname() << "] ";
    pending_inbound_ifname_.clear();
  }

  if (!inbound_configured_)
    return;

  LOG(INFO) << "Disabling inbound for " << ifname_ << " ["
            << config_.arc_ifname() << "] ";

  if (process_runner_->Run(
          {kIpTablesPath, "-t", "nat", "-F", "try_arc", "-w"}) != 0) {
    LOG(DFATAL) << "Could not update iptables";
  }

  inbound_configured_ = false;
}

std::ostream& operator<<(std::ostream& stream, const ArcIpConfig& conf) {
  stream << "ArcIpConfig "
         << "{ netns: " << conf.con_netns_
         << ", bridge iface: " << conf.config_.br_ifname()
         << ", guest iface: " << conf.config_.arc_ifname()
         << ", inbound iface: " << conf.ifname_
         << ", ipv6: " << conf.ipv6_address_full_
         << ", gateway: " << conf.ipv6_router_ << " }";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const struct in_addr& addr) {
  char buf[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &addr, buf, sizeof(buf));
  stream << buf;
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const struct in6_addr& addr) {
  char buf[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
  stream << buf;
  return stream;
}

}  // namespace arc_networkd
