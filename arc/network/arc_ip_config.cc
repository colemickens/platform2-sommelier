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
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "arc/network/scoped_ns.h"

namespace {

const int kInvalidNs = 0;
const int kInvalidTableId = -1;
const char kDefaultNetmask[] = "255.255.255.252";

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
  LOG(INFO) << "Setting up " << ifname_ << " " << config_.br_ifname() << " "
            << config_.arc_ifname();

  // Configure the persistent Chrome OS bridge interface with static IP.
  process_runner_->Run({kBrctlPath, "addbr", config_.br_ifname()});
  process_runner_->Run({kIfConfigPath, config_.br_ifname(), config_.br_ipv4(),
                        "netmask", kDefaultNetmask, "up"});
  // See nat.conf in chromeos-nat-init for the rest of the NAT setup rules.
  process_runner_->Run({kIpTablesPath, "-t", "mangle", "-A", "PREROUTING", "-i",
                        config_.br_ifname(), "-j", "MARK", "--set-mark", "1",
                        "-w"});

  // The legacy ARC device is configured differently.
  if (ifname_ == kAndroidDevice) {
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
  } else {
    // TODO(garrick): Add iptables for host devices.
  }
}

void ArcIpConfig::Teardown() {
  LOG(INFO) << "Tearing down " << ifname_ << " " << config_.br_ifname() << " "
            << config_.arc_ifname();
  Clear();
  DisableInbound();

  if (ifname_ == kAndroidDevice) {
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
  } else {
    // TODO(garrick): Undo
  }

  process_runner_->Run({kIpTablesPath, "-t", "mangle", "-D", "PREROUTING", "-i",
                        config_.br_ifname(), "-j", "MARK", "--set-mark", "1",
                        "-w"});

  process_runner_->Run({kIfConfigPath, config_.br_ifname(), "down"});
  process_runner_->Run({kBrctlPath, "delbr", config_.br_ifname()});
}

bool ArcIpConfig::Init(pid_t con_netns) {
  con_netns_ = con_netns;
  const std::string pid = base::IntToString(con_netns_);
  const std::string veth = "veth_" + ifname_;
  const std::string peer = "peer_" + ifname_;

  if (!con_netns_) {
    LOG(INFO) << "Uninitializing " << con_netns_ << " " << ifname_ << " "
              << config_.br_ifname() << " " << config_.arc_ifname();
    return true;
  }

  LOG(INFO) << "Initializing " << con_netns_ << " " << ifname_ << " "
            << config_.br_ifname() << " " << config_.arc_ifname();

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
  if (ifname_ == kAndroidDevice) {
    process_runner_->WriteSentinelToContainer(pid);
  }
  return true;
}

void ArcIpConfig::ContainerReady(bool ready) {
  if (!if_up_ && ready)
    LOG(INFO) << config_.arc_ifname() << " is now up.";
  if_up_ = ready;
  if (if_up_ && !pending_inbound_ifname_.empty()) {
    std::string ifname = std::move(pending_inbound_ifname_);
    EnableInbound(ifname);
  }
}

int ArcIpConfig::GetTableIdForInterface(const std::string& ifname) {
  base::FilePath ifindex_path(base::StringPrintf(
      "/proc/%d/root/sys/class/net/%s/ifindex", con_netns_, ifname.c_str()));
  std::string contents;
  if (!base::ReadFileToString(ifindex_path, &contents)) {
    PLOG(ERROR) << "Could not read " << ifindex_path.value();
    return kInvalidTableId;
  }
  base::TrimWhitespaceASCII(contents, base::TRIM_TRAILING, &contents);
  int table_id = kInvalidTableId;
  if (!base::StringToInt(contents, &table_id)) {
    LOG(ERROR) << "Could not parse ifindex from " << ifindex_path.value()
               << ": " << contents;
    return kInvalidTableId;
  }
  // Android adds a constant to the interface index to derive the table id.
  // This is defined in system/netd/server/RouteController.h
  constexpr int kRouteControllerRouteTableOffsetFromIndex = 1000;
  table_id += kRouteControllerRouteTableOffsetFromIndex;

  LOG(INFO) << "Found table id " << table_id << " for iface " << ifname;

  return table_id;
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

bool ArcIpConfig::Set(const struct in6_addr& address,
                      int prefix_len,
                      const struct in6_addr& router_addr,
                      const std::string& lan_ifname) {
  Clear();

  if (!if_up_) {
    LOG(ERROR) << "Cannot set IPv6 address: container interface "
               << config_.arc_ifname() << " not ready.";
    return false;
  }

  // At this point, arc0 is up and the LAN interface has been up for several
  // seconds. If the routing table name has not yet been populated,
  // something really bad probably happened on the Android side.
  if (routing_table_id_ == kInvalidTableId) {
    routing_table_id_ = GetTableIdForInterface(config_.arc_ifname());
    if (routing_table_id_ == kInvalidTableId) {
      LOG(FATAL) << "Could not look up routing table ID for "
                 << config_.arc_ifname();
    }
  }

  char buf[INET6_ADDRSTRLEN];

  CHECK(inet_ntop(AF_INET6, &address, buf, sizeof(buf)));
  ipv6_address_ = buf;
  ipv6_address_full_ = ipv6_address_;
  ipv6_address_full_.append("/" + std::to_string(prefix_len));

  CHECK(inet_ntop(AF_INET6, &router_addr, buf, sizeof(buf)));
  ipv6_router_ = buf;

  // This is needed to support the single network legacy case.
  // If this isn't the legacy device, then ensure the interface is the same.
  CHECK(ifname_ == kAndroidDevice || ifname_ == lan_ifname);
  ipv6_dev_ifname_ = lan_ifname;

  {
    ScopedNS ns(con_netns_);
    if (ns.IsValid()) {
      VLOG(1) << "Setting " << *this;

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

  CHECK_EQ(process_runner_->Run({kIp6TablesPath, "-A", "FORWARD", "-i",
                                 ipv6_dev_ifname_, "-o", config_.br_ifname(),
                                 "-j", "ACCEPT", "-w"}),
           0);

  CHECK_EQ(process_runner_->Run({kIp6TablesPath, "-A", "FORWARD", "-i",
                                 config_.br_ifname(), "-o", ipv6_dev_ifname_,
                                 "-j", "ACCEPT", "-w"}),
           0);

  ipv6_configured_ = true;
  return true;
}

bool ArcIpConfig::Clear() {
  if (!ipv6_configured_)
    return true;

  VLOG(1) << "Clearing " << *this;

  // These should never fail.
  CHECK_EQ(process_runner_->Run({kIp6TablesPath, "-D", "FORWARD", "-i",
                                 config_.br_ifname(), "-o", ipv6_dev_ifname_,
                                 "-j", "ACCEPT", "-w"}),
           0);

  CHECK_EQ(process_runner_->Run({kIp6TablesPath, "-D", "FORWARD", "-i",
                                 ipv6_dev_ifname_, "-o", config_.br_ifname(),
                                 "-j", "ACCEPT", "-w"}),
           0);

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
                            std::to_string(routing_table_id_)});

      process_runner_->Run({kIpPath, "-6", "route", "del", ipv6_router_, "dev",
                            config_.arc_ifname(), "table",
                            std::to_string(routing_table_id_)});

      // This often fails because ARC tries to delete the address on its own
      // when it is notified that the LAN is down.

      process_runner_->Run({kIpPath, "-6", "addr", "del", ipv6_address_full_,
                            "dev", config_.arc_ifname()},
                           false);
    }
  }

  ipv6_dev_ifname_.clear();
  ipv6_configured_ = false;
  return true;
}

void ArcIpConfig::EnableInbound(const std::string& lan_ifname) {
  if (!if_up_) {
    LOG(INFO) << "Enable inbound for " << ifname_ << " ["
              << config_.arc_ifname() << "]"
              << " on " << lan_ifname << " pending on container interface up.";
    pending_inbound_ifname_ = lan_ifname;
    return;
  }

  DisableInbound();

  // TODO(garrick): This only applies for arc0 for now. Clean this up.
  if (ifname_ == kAndroidDevice) {
    LOG(INFO) << "Enabling inbound for " << ifname_ << " ["
              << config_.arc_ifname() << "]"
              << " on " << lan_ifname;

    CHECK_EQ(process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "try_arc",
                                   "-i", lan_ifname, "-j", "dnat_arc", "-w"}),
             0);
  }

  inbound_configured_ = true;
  DCHECK(pending_inbound_ifname_.empty());
}

void ArcIpConfig::DisableInbound() {
  if (!pending_inbound_ifname_.empty()) {
    LOG(INFO) << "Clearing pending inbound request for " << ifname_ << " ["
              << config_.arc_ifname() << "] ";
    pending_inbound_ifname_.clear();
  }

  if (!inbound_configured_)
    return;

  // TODO(garrick): This only applies for arc0 for now. Clean this up.
  if (ifname_ == kAndroidDevice) {
    LOG(INFO) << "Disabling inbound for " << ifname_ << " ["
              << config_.arc_ifname() << "] ";

    CHECK_EQ(process_runner_->Run(
                 {kIpTablesPath, "-t", "nat", "-F", "try_arc", "-w"}),
             0);
  }

  inbound_configured_ = false;
}

std::ostream& operator<<(std::ostream& stream, const ArcIpConfig& conf) {
  stream << "ArcIpConfig "
         << "{ netns=" << conf.con_netns_
         << ", host iface=" << conf.config_.br_ifname()
         << ", guest iface=" << conf.config_.arc_ifname()
         << ", inbound iface=" << conf.ifname_
         << ", ipv6=" << conf.ipv6_address_full_
         << ", gateway=" << conf.ipv6_router_ << " }";
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
