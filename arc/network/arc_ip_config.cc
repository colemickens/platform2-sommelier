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

const int kInvalidNs = 0;
const int kInvalidTableId = -1;
const char kDefaultNetmask[] = "255.255.255.252";
const char kSentinelFile[] = "/dev/.arc_network_ready";

const char kUnprivilegedUser[] = "nobody";
const uint64_t kIpTablesCapMask =
    CAP_TO_MASK(CAP_NET_ADMIN) | CAP_TO_MASK(CAP_NET_RAW);

// These match what is used in iptables.cc in firewalld.
const char kBrctlPath[] = "/sbin/brctl";
const char kIfConfigPath[] = "/bin/ifconfig";
const char kIpPath[] = "/bin/ip";
const char kIpTablesPath[] = "/sbin/iptables";
const char kIp6TablesPath[] = "/sbin/ip6tables";
const char kNsEnterPath[] = "/usr/bin/nsenter";
const char kTouchPath[] = "/system/bin/touch";

// Enforces the expected processes are run with the correct privileges.
class JailedProcessRunner {
 public:
  JailedProcessRunner() = default;
  ~JailedProcessRunner() = default;

  int Run(const std::vector<std::string>& argv, bool log_failures = true) {
    brillo::Minijail* m = brillo::Minijail::GetInstance();
    minijail* jail = m->New();
    CHECK(m->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser));
    m->UseCapabilities(jail, kIpTablesCapMask);
    return RunSyncDestroy(argv, m, jail, log_failures);
  }

  int AddInterfaceToContainer(const std::string& host_ifname,
                              const std::string& con_ifname,
                              const std::string& con_pid) {
    brillo::Minijail* m = brillo::Minijail::GetInstance();
    minijail* jail = m->New();
    return RunSyncDestroy({kNsEnterPath, "-t", con_pid, "-n", "--", kIpPath,
                           "link", "set", host_ifname, "name", con_ifname},
                          m, jail, true);
  }

  int WriteSentinelToContainer(const std::string& con_pid) {
    brillo::Minijail* m = brillo::Minijail::GetInstance();
    minijail* jail = m->New();
    return RunSyncDestroy({kNsEnterPath, "-t", con_pid, "--mount", "--pid",
                           "--", kTouchPath, kSentinelFile},
                          m, jail, true);
  }

 private:
  int RunSyncDestroy(const std::vector<std::string>& argv,
                     brillo::Minijail* mj,
                     minijail* jail,
                     bool log_failures) {
    std::vector<char*> args;
    for (const auto& arg : argv) {
      args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);

    int status;
    bool ran = mj->RunSyncAndDestroy(jail, args, &status);
    if (!ran) {
      LOG(ERROR) << "Could not execute '" << base::JoinString(argv, " ") << "'";
    } else if (log_failures &&
               (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
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

  DISALLOW_COPY_AND_ASSIGN(JailedProcessRunner);
};

}  // namespace

namespace arc_networkd {

ArcIpConfig::ArcIpConfig(const std::string& ifname, const DeviceConfig& config)
    : ifname_(ifname),
      config_(config),
      con_netns_(kInvalidNs),
      routing_table_id_(kInvalidTableId),
      ipv6_dev_ifname_(ifname) {
  Setup();
}

ArcIpConfig::~ArcIpConfig() {
  Teardown();
}

void ArcIpConfig::Setup() {
  LOG(INFO) << "Setting up " << ifname_ << " " << config_.br_ifname() << " "
            << config_.arc_ifname();

  JailedProcessRunner proc;
  // Configure the persistent Chrome OS bridge interface with static IP.
  proc.Run({kBrctlPath, "addbr", config_.br_ifname()});
  proc.Run({kIfConfigPath, config_.br_ifname(), config_.br_ipv4(), "netmask",
            kDefaultNetmask, "up"});
  // See nat.conf in chromeos-nat-init for the rest of the NAT setup rules.
  proc.Run({kIpTablesPath, "-t", "mangle", "-A", "PREROUTING", "-i",
            config_.br_ifname(), "-j", "MARK", "--set-mark", "1", "-w"});

  // The legacy ARC device is configured differently.
  if (ifname_ == kAndroidDevice) {
    // Sanity check.
    CHECK_EQ("arcbr0", config_.br_ifname());
    CHECK_EQ("arc0", config_.arc_ifname());

    // Forward "unclaimed" packets to Android to allow inbound connections
    // from devices on the LAN.
    proc.Run({kIpTablesPath, "-t", "nat", "-N", "dnat_arc", "-w"});
    proc.Run({kIpTablesPath, "-t", "nat", "-A", "dnat_arc", "-j", "DNAT",
              "--to-destination", config_.arc_ipv4(), "-w"});

    // This chain is dynamically updated whenever the default interface changes.
    proc.Run({kIpTablesPath, "-t", "nat", "-N", "try_arc", "-w"});
    proc.Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-m", "socket",
              "--nowildcard", "-j", "ACCEPT", "-w"});
    proc.Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-p", "tcp", "-j",
              "try_arc", "-w"});
    proc.Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING", "-p", "udp", "-j",
              "try_arc", "-w"});

    proc.Run({kIpTablesPath, "-t", "filter", "-A", "FORWARD", "-o",
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

  JailedProcessRunner proc;
  if (ifname_ == kAndroidDevice) {
    proc.Run({kIpTablesPath, "-t", "filter", "-D", "FORWARD", "-o",
              config_.br_ifname(), "-j", "ACCEPT", "-w"});

    proc.Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-p", "udp", "-j",
              "try_arc", "-w"});
    proc.Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-p", "tcp", "-j",
              "try_arc", "-w"});
    proc.Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-m", "socket",
              "--nowildcard", "-j", "ACCEPT", "-w"});

    proc.Run({kIpTablesPath, "-t", "nat", "-F", "try_arc", "-w"});
    proc.Run({kIpTablesPath, "-t", "nat", "-X", "try_arc", "-w"});

    proc.Run({kIpTablesPath, "-t", "nat", "-F", "dnat_arc", "-w"});
    proc.Run({kIpTablesPath, "-t", "nat", "-X", "dnat_arc", "-w"});
  } else {
    // TODO(garrick): Undo
  }

  proc.Run({kIpTablesPath, "-t", "mangle", "-D", "PREROUTING", "-i",
            config_.br_ifname(), "-j", "MARK", "--set-mark", "1", "-w"});

  proc.Run({kIfConfigPath, config_.br_ifname(), "down"});
  proc.Run({kBrctlPath, "delbr", config_.br_ifname()});
}

bool ArcIpConfig::Init(pid_t con_netns) {
  con_netns_ = con_netns;
  const std::string pid = base::IntToString(con_netns_);
  const std::string veth = "veth_" + ifname_;
  const std::string peer = "peer_" + ifname_;

  JailedProcessRunner proc;
  if (!con_netns_) {
    LOG(INFO) << "Uninitializing " << con_netns_ << " " << ifname_ << " "
              << config_.br_ifname() << " " << config_.arc_ifname();

    proc.Run({kBrctlPath, "delif", config_.br_ifname(), veth});
    proc.Run({kIfConfigPath, veth, "down"});
    proc.Run({kIpPath, "link", "delete", veth, "type", "veth"});
    self_netns_fd_.reset();
    con_netns_fd_.reset();
    return true;
  }

  LOG(INFO) << "Initializing " << con_netns_ << " " << ifname_ << " "
            << config_.br_ifname() << " " << config_.arc_ifname();

  const std::string filename =
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

  proc.Run({kIpPath, "link", "delete", veth}, false /* log_failures */);
  proc.Run(
      {kIpPath, "link", "add", veth, "type", "veth", "peer", "name", peer});
  proc.Run({kIfConfigPath, veth, "up"});
  proc.Run({kIpPath, "link", "set", "dev", peer, "addr", config_.mac_addr(),
            "down"});
  proc.Run({kBrctlPath, "addif", config_.br_ifname(), veth});

  // Container ns needs to be ready here. For now this is gated by the wait loop
  // the conf file.
  // TODO(garrick): Run this in response to the RTNETLINK (NEWNSID) event:
  // https://elixir.bootlin.com/linux/v4.14/source/net/core/net_namespace.c#L234

  proc.Run({kIpPath, "link", "set", peer, "netns", pid});
  proc.AddInterfaceToContainer(peer, config_.arc_ifname(), pid);

  // Signal the container that the network device is ready.
  // This is only applicable for arc0.
  if (ifname_ == kAndroidDevice) {
    proc.WriteSentinelToContainer(pid);
  }
  return true;
}

bool ArcIpConfig::ContainerInit() {
  if (!con_netns_) {
    LOG(WARNING) << "No netns";
    return false;
  }

  PCHECK(setns(con_netns_fd_.get(), CLONE_NEWNET) == 0);
  base::ScopedFD fd(socket(AF_INET, SOCK_DGRAM, IPPROTO_IP));
  PCHECK(setns(self_netns_fd_.get(), CLONE_NEWNET) == 0);

  if (!fd.is_valid()) {
    LOG(ERROR) << "socket() failed";
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, config_.arc_ifname().c_str(), IFNAMSIZ);

  if (ioctl(fd.get(), SIOCGIFFLAGS, &ifr) < 0) {
    PLOG(ERROR) << "SIOCGIFFLAGS(" << config_.arc_ifname() << ") failed";
    return false;
  }

  if (!(ifr.ifr_flags & IFF_UP)) {
    return false;
  }

  return true;
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

  if (!con_netns_fd_.is_valid() || !self_netns_fd_.is_valid()) {
    LOG(ERROR) << "Cannot set IPv6 address: no netns configured";
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

  PCHECK(setns(con_netns_fd_.get(), CLONE_NEWNET) == 0);

  VLOG(1) << "Setting " << *this;

  // These can fail if the interface disappears (e.g. hot-unplug).
  // If that happens, the error will be logged, because sometimes it
  // might help in debugging a real issue.

  JailedProcessRunner proc;
  proc.Run({kIpPath, "-6", "addr", "add", ipv6_address_full_, "dev",
            config_.arc_ifname()});

  proc.Run({kIpPath, "-6", "route", "add", ipv6_router_, "dev",
            config_.arc_ifname(), "table", std::to_string(routing_table_id_)});

  proc.Run({kIpPath, "-6", "route", "add", "default", "via", ipv6_router_,
            "dev", config_.arc_ifname(), "table",
            std::to_string(routing_table_id_)});

  PCHECK(setns(self_netns_fd_.get(), CLONE_NEWNET) == 0);

  proc.Run({kIpPath, "-6", "route", "add", ipv6_address_full_, "dev",
            config_.br_ifname()});

  proc.Run({kIpPath, "-6", "neigh", "add", "proxy", ipv6_address_, "dev",
            ipv6_dev_ifname_});

  // These should never fail.

  CHECK_EQ(proc.Run({kIp6TablesPath, "-A", "FORWARD", "-i", ipv6_dev_ifname_,
                     "-o", config_.br_ifname(), "-j", "ACCEPT", "-w"}),
           0);

  CHECK_EQ(proc.Run({kIp6TablesPath, "-A", "FORWARD", "-i", config_.br_ifname(),
                     "-o", ipv6_dev_ifname_, "-j", "ACCEPT", "-w"}),
           0);

  ipv6_configured_ = true;
  return true;
}

bool ArcIpConfig::Clear() {
  if (!ipv6_configured_)
    return true;

  VLOG(1) << "Clearing " << *this;

  // These should never fail.
  JailedProcessRunner proc;
  CHECK_EQ(proc.Run({kIp6TablesPath, "-D", "FORWARD", "-i", config_.br_ifname(),
                     "-o", ipv6_dev_ifname_, "-j", "ACCEPT", "-w"}),
           0);

  CHECK_EQ(proc.Run({kIp6TablesPath, "-D", "FORWARD", "-i", ipv6_dev_ifname_,
                     "-o", config_.br_ifname(), "-j", "ACCEPT", "-w"}),
           0);

  // This often fails because the kernel removes the proxy entry automatically.

  proc.Run({kIpPath, "-6", "neigh", "del", "proxy", ipv6_address_, "dev",
            ipv6_dev_ifname_},
           false /* log_failures */);

  // This can fail if the interface disappears (e.g. hot-unplug).  Rare.

  proc.Run({kIpPath, "-6", "route", "del", ipv6_address_full_, "dev",
            config_.br_ifname()});

  PCHECK(setns(con_netns_fd_.get(), CLONE_NEWNET) == 0);

  proc.Run({kIpPath, "-6", "route", "del", "default", "via", ipv6_router_,
            "dev", config_.arc_ifname(), "table",
            std::to_string(routing_table_id_)});

  proc.Run({kIpPath, "-6", "route", "del", ipv6_router_, "dev",
            config_.arc_ifname(), "table", std::to_string(routing_table_id_)});

  // This often fails because ARC tries to delete the address on its own
  // when it is notified that the LAN is down.

  proc.Run({kIpPath, "-6", "addr", "del", ipv6_address_full_, "dev",
            config_.arc_ifname()},
           false);

  PCHECK(setns(self_netns_fd_.get(), CLONE_NEWNET) == 0);

  ipv6_dev_ifname_.clear();
  ipv6_configured_ = false;
  return true;
}

void ArcIpConfig::EnableInbound(const std::string& lan_ifname) {
  LOG(INFO) << "Enabling inbound for " << ifname_ << " on " << lan_ifname;
  DisableInbound();

  VLOG(1) << "Enabling inbound for " << *this;

  JailedProcessRunner proc;
  CHECK_EQ(proc.Run({kIpTablesPath, "-t", "nat", "-A", "try_arc", "-i",
                     lan_ifname, "-j", "dnat_arc", "-w"}),
           0);
  inbound_configured_ = true;
}

void ArcIpConfig::DisableInbound() {
  if (!inbound_configured_)
    return;

  VLOG(1) << "Disabling inbound for " << *this;

  JailedProcessRunner proc;
  CHECK_EQ(proc.Run({kIpTablesPath, "-t", "nat", "-F", "try_arc", "-w"}), 0);
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
