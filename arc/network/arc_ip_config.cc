// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_ip_config.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/capability.h>
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

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/minijail/minijail.h>
#include <brillo/process.h>

namespace {

const int kInvalidTableId = -1;

// These match what is used in iptables.cc in firewalld.
const char kIpPath[] = "/bin/ip";
const char kIpTablesPath[] = "/sbin/iptables";
const char kIp6TablesPath[] = "/sbin/ip6tables";
const char kUnprivilegedUser[] = "nobody";
const uint64_t kIpTablesCapMask =
    CAP_TO_MASK(CAP_NET_ADMIN) | CAP_TO_MASK(CAP_NET_RAW);

}  // namespace

namespace arc_networkd {

ArcIpConfig::ArcIpConfig(const std::string& int_ifname,
                         const std::string& con_ifname,
                         pid_t con_netns)
    : int_ifname_(int_ifname),
      con_ifname_(con_ifname),
      con_netns_(con_netns),
      routing_table_id_(kInvalidTableId) {}

ArcIpConfig::~ArcIpConfig() {
  Clear();
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

bool ArcIpConfig::Init() {
  if (!con_netns_)
    return true;

  std::string filename =
      base::StringPrintf("/proc/%d/ns/net", static_cast<int>(con_netns_));
  con_netns_fd_.reset(open(filename.c_str(), O_RDONLY));
  if (!con_netns_fd_.is_valid()) {
    PLOG(ERROR) << "Could not open " << filename;
    return false;
  }

  self_netns_fd_.reset(open("/proc/self/ns/net", O_RDONLY));
  if (!self_netns_fd_.is_valid()) {
    PLOG(ERROR) << "Could not open host netns";
    return false;
  }

  return true;
}

bool ArcIpConfig::ContainerInit() {
  if (!con_netns_)
    return true;

  PCHECK(setns(con_netns_fd_.get(), CLONE_NEWNET) == 0);
  base::ScopedFD fd(socket(AF_INET, SOCK_DGRAM, IPPROTO_IP));
  PCHECK(setns(self_netns_fd_.get(), CLONE_NEWNET) == 0);

  if (!fd.is_valid()) {
    LOG(ERROR) << "socket() failed";
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, con_ifname_.c_str(), IFNAMSIZ);

  if (ioctl(fd.get(), SIOCGIFFLAGS, &ifr) < 0) {
    PLOG(ERROR) << "SIOCGIFFLAGS(" << con_ifname_ << ") failed";
    return false;
  }

  if (!(ifr.ifr_flags & IFF_UP))
    return false;

  return true;
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

// Runs |argv| (a program name + argument list) with reduced privileges.
// Returns "raw" status on success, or -1 if the program could not be executed.
int ArcIpConfig::StartProcessInMinijail(const std::vector<std::string>& argv,
                                        bool log_failures) {
  brillo::Minijail* m = brillo::Minijail::GetInstance();
  minijail* jail = m->New();

  CHECK(m->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser));
  m->UseCapabilities(jail, kIpTablesCapMask);

  std::vector<char*> args;
  for (const auto& arg : argv) {
    args.push_back(const_cast<char*>(arg.c_str()));
  }
  args.push_back(nullptr);

  int status;
  bool ran = m->RunSyncAndDestroy(jail, args, &status);

  if (!ran) {
    LOG(ERROR) << "Could not execute '" << base::JoinString(argv, " ") << "'";
  } else if (log_failures && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
    if (WIFEXITED(status)) {
      LOG(WARNING) << "Subprocess '" << base::JoinString(argv, " ")
                   << "' exited with code " << WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      LOG(WARNING) << "Subprocess '" << base::JoinString(argv, " ")
                   << "' exited with signal " << WTERMSIG(status);
    } else {
      LOG(WARNING) << "Subprocess '" << base::JoinString(argv, " ")
                   << "' exited with unknown status " << status;
    }
  }

  return ran && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool ArcIpConfig::Set(const struct in6_addr& address,
                      int prefix_len,
                      const struct in6_addr& router_addr,
                      const std::string& lan_ifname) {
  Clear();

  if (!con_netns_fd_.is_valid() || !self_netns_fd_.is_valid()) {
    LOG(ERROR) << "Cannot set IPv6 address: no netns configured";
    return false;
  }

  // At this point, arc0 is up and the LAN interface has been up for several
  // seconds.  If the routing table name has not yet been populated,
  // something really bad probably happened on the Android side.
  if (routing_table_id_ == kInvalidTableId) {
    routing_table_id_ = GetTableIdForInterface(con_ifname_);
    if (routing_table_id_ == kInvalidTableId) {
      LOG(FATAL) << "Could not look up routing table ID for " << con_ifname_;
    }
  }

  char buf[INET6_ADDRSTRLEN];

  CHECK(inet_ntop(AF_INET6, &address, buf, sizeof(buf)));
  current_address_ = buf;
  current_address_full_ = current_address_;
  current_address_full_.append("/" + std::to_string(prefix_len));

  CHECK(inet_ntop(AF_INET6, &router_addr, buf, sizeof(buf)));
  current_router_ = buf;

  current_lan_ifname_ = lan_ifname;

  PCHECK(setns(con_netns_fd_.get(), CLONE_NEWNET) == 0);

  VLOG(1) << "Setting " << *this;

  // These can fail if the interface disappears (e.g. hot-unplug).
  // If that happens, the error will be logged, because sometimes it
  // might help in debugging a real issue.

  StartProcessInMinijail(
      {kIpPath, "-6", "addr", "add", current_address_full_, "dev", con_ifname_},
      true /* log_failures */);

  StartProcessInMinijail(
      {kIpPath, "-6", "route", "add", current_router_, "dev", con_ifname_,
       "table", std::to_string(routing_table_id_)},
      true);

  StartProcessInMinijail(
      {kIpPath, "-6", "route", "add", "default", "via", current_router_, "dev",
       con_ifname_, "table", std::to_string(routing_table_id_)},
      true);

  PCHECK(setns(self_netns_fd_.get(), CLONE_NEWNET) == 0);

  StartProcessInMinijail({kIpPath, "-6", "route", "add", current_address_full_,
                          "dev", int_ifname_},
                         true);

  StartProcessInMinijail({kIpPath, "-6", "neigh", "add", "proxy",
                          current_address_, "dev", current_lan_ifname_},
                         true);

  // These should never fail.

  CHECK_EQ(StartProcessInMinijail(
               {kIp6TablesPath, "-A", "FORWARD", "-i", current_lan_ifname_,
                "-o", int_ifname_, "-j", "ACCEPT", "-w"},
               true),
           0);

  CHECK_EQ(StartProcessInMinijail(
               {kIp6TablesPath, "-A", "FORWARD", "-i", int_ifname_, "-o",
                current_lan_ifname_, "-j", "ACCEPT", "-w"},
               true),
           0);

  ipv6_configured_ = true;
  return true;
}

bool ArcIpConfig::Clear() {
  if (!ipv6_configured_)
    return true;

  VLOG(1) << "Clearing " << *this;

  // These should never fail.

  CHECK_EQ(StartProcessInMinijail(
               {kIp6TablesPath, "-D", "FORWARD", "-i", int_ifname_, "-o",
                current_lan_ifname_, "-j", "ACCEPT", "-w"},
               true),
           0);

  CHECK_EQ(StartProcessInMinijail(
               {kIp6TablesPath, "-D", "FORWARD", "-i", current_lan_ifname_,
                "-o", int_ifname_, "-j", "ACCEPT", "-w"},
               true),
           0);

  // This often fails because the kernel removes the proxy entry automatically.

  StartProcessInMinijail({kIpPath, "-6", "neigh", "del", "proxy",
                          current_address_, "dev", current_lan_ifname_},
                         false /* log_failures */);

  // This can fail if the interface disappears (e.g. hot-unplug).  Rare.

  StartProcessInMinijail({kIpPath, "-6", "route", "del", current_address_full_,
                          "dev", int_ifname_},
                         true);

  PCHECK(setns(con_netns_fd_.get(), CLONE_NEWNET) == 0);

  StartProcessInMinijail(
      {kIpPath, "-6", "route", "del", "default", "via", current_router_, "dev",
       con_ifname_, "table", std::to_string(routing_table_id_)},
      true);

  StartProcessInMinijail(
      {kIpPath, "-6", "route", "del", current_router_, "dev", con_ifname_,
       "table", std::to_string(routing_table_id_)},
      true);

  // This often fails because ARC tries to delete the address on its own
  // when it is notified that the LAN is down.

  StartProcessInMinijail(
      {kIpPath, "-6", "addr", "del", current_address_full_, "dev", con_ifname_},
      false);

  PCHECK(setns(self_netns_fd_.get(), CLONE_NEWNET) == 0);

  ipv6_configured_ = false;
  return true;
}

void ArcIpConfig::EnableInbound(const std::string& lan_ifname) {
  DisableInbound();

  VLOG(1) << "Enabling inbound for " << *this;

  CHECK_EQ(StartProcessInMinijail({kIpTablesPath, "-t", "nat", "-A", "try_arc",
                                   "-i", lan_ifname, "-j", "dnat_arc", "-w"},
                                  true),
           0);
  inbound_configured_ = true;
}

void ArcIpConfig::DisableInbound() {
  if (!inbound_configured_)
    return;

  VLOG(1) << "Disabling inbound for " << *this;

  CHECK_EQ(StartProcessInMinijail(
               {kIpTablesPath, "-t", "nat", "-F", "try_arc", "-w"}, true),
           0);
  inbound_configured_ = false;
}

std::ostream& operator<<(std::ostream& stream, const ArcIpConfig& conf) {
  stream << "ArcIpConfig "
         << "{ netns=" << conf.con_netns_
         << ", host iface=" << conf.int_ifname_
         << ", guest iface=" << conf.con_ifname_
         << ", inbound iface=" << conf.current_lan_ifname_
         << ", ipv6=" << conf.current_address_full_
         << ", gateway=" << conf.current_router_
         << " }";
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
