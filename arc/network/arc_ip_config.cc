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

#include "arc/network/datapath.h"
#include "arc/network/scoped_ns.h"

namespace {

constexpr int kInvalidNs = 0;
constexpr int kInvalidTableId = -1;
constexpr int kMaxTableRetries = 10;  // Based on 1 second delay.
constexpr base::TimeDelta kTableRetryDelay = base::TimeDelta::FromSeconds(1);

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

}  // namespace

namespace arc_networkd {

ArcIpConfig::ArcIpConfig(const std::string& ifname, const DeviceConfig& config)
    : ArcIpConfig(ifname,
                  config,
                  std::make_unique<MinijailedProcessRunner>(),
                  nullptr) {}

ArcIpConfig::ArcIpConfig(
    const std::string& ifname,
    const DeviceConfig& config,
    std::unique_ptr<MinijailedProcessRunner> process_runner,
    std::unique_ptr<Datapath> datapath)
    : ifname_(ifname),
      config_(config),
      con_netns_(kInvalidNs),
      routing_table_id_(kInvalidTableId),
      routing_table_attempts_(0),
      ipv6_configured_(false),
      ipv6_dev_ifname_(ifname),
      process_runner_(std::move(process_runner)) {
  if (!datapath) {
    datapath = std::make_unique<Datapath>(process_runner_.get());
  }
  datapath_ = std::move(datapath);
}

ArcIpConfig::~ArcIpConfig() {
  Clear();
}

bool ArcIpConfig::Init(pid_t con_netns) {
  if (!con_netns) {
    LOG(INFO) << "Uninitializing " << ifname_
              << " bridge: " << config_.br_ifname()
              << " guest_iface: " << config_.arc_ifname();
    con_netns_ = kInvalidNs;
    return true;
  }

  con_netns_ = con_netns;

  return true;
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

  // If this device config has not yet been initialized then just return.
  // Currently, it should only be the case that this function is triggered
  // after both sides of the veth pair are up.
  if (con_netns_ == kInvalidNs) {
    LOG(DFATAL) << "Invalid container";
    return;
  }

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

  // These can fail if the interface has already been removed, which will happen
  // as long as there is this temporary split between arc_service and arc_helper

  process_runner_->Run({kIpPath, "-6", "route", "del", ipv6_address_full_,
                        "dev", config_.br_ifname()},
                       false);

  {
    ScopedNS ns(con_netns_);
    if (ns.IsValid()) {
      process_runner_->Run(
          {kIpPath, "-6", "route", "del", "default", "via", ipv6_router_, "dev",
           config_.arc_ifname(), "table", std::to_string(routing_tid)},
          false);

      process_runner_->Run(
          {kIpPath, "-6", "route", "del", ipv6_router_, "dev",
           config_.arc_ifname(), "table", std::to_string(routing_tid)},
          false);

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
