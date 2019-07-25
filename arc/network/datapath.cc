// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/datapath.h"

#include <base/strings/string_number_conversions.h>

namespace arc_networkd {

constexpr char kDefaultNetmask[] = "255.255.255.252";

std::string ArcVethHostName(std::string ifname) {
  return "veth_" + ifname;
}

std::string ArcVethPeerName(std::string ifname) {
  return "peer_" + ifname;
}

Datapath::Datapath(MinijailedProcessRunner* process_runner)
    : process_runner_(process_runner) {
  CHECK(process_runner_);
}

MinijailedProcessRunner& Datapath::runner() const {
  return *process_runner_;
}

bool Datapath::AddBridge(const std::string& ifname,
                         const std::string& ipv4_addr) {
  // Configure the persistent Chrome OS bridge interface with static IP.
  if (process_runner_->Run({kBrctlPath, "addbr", ifname}) != 0) {
    return false;
  }

  if (process_runner_->Run({kIfConfigPath, ifname, ipv4_addr, "netmask",
                            kDefaultNetmask, "up"}) != 0) {
    RemoveBridge(ifname);
    return false;
  }

  // See nat.conf in chromeos-nat-init for the rest of the NAT setup rules.
  if (process_runner_->Run({kIpTablesPath, "-t", "mangle", "-A", "PREROUTING",
                            "-i", ifname, "-j", "MARK", "--set-mark", "1",
                            "-w"}) != 0) {
    RemoveBridge(ifname);
    return false;
  }

  return true;
}

void Datapath::RemoveBridge(const std::string& ifname) {
  process_runner_->Run({kIpTablesPath, "-t", "mangle", "-D", "PREROUTING", "-i",
                        ifname, "-j", "MARK", "--set-mark", "1", "-w"});
  process_runner_->Run({kIfConfigPath, ifname, "down"});
  process_runner_->Run({kBrctlPath, "delbr", ifname});
}

std::string Datapath::AddVirtualBridgedInterface(const std::string& ifname,
                                                 const std::string& mac_addr,
                                                 const std::string& br_ifname) {
  const std::string veth = ArcVethHostName(ifname);
  const std::string peer = ArcVethPeerName(ifname);

  RemoveInterface(veth);
  if (process_runner_->Run({kIpPath, "link", "add", veth, "type", "veth",
                            "peer", "name", peer}) != 0) {
    return "";
  }

  if (process_runner_->Run({kIfConfigPath, veth, "up"}) != 0) {
    RemoveInterface(veth);
    RemoveInterface(peer);
    return "";
  }

  if (process_runner_->Run({kIpPath, "link", "set", "dev", peer, "addr",
                            mac_addr, "down"}) != 0) {
    RemoveInterface(veth);
    RemoveInterface(peer);
    return "";
  }

  if (process_runner_->Run({kBrctlPath, "addif", br_ifname, veth}) != 0) {
    RemoveInterface(veth);
    RemoveInterface(peer);
    return "";
  }

  return peer;
}

void Datapath::RemoveInterface(const std::string& ifname) {
  process_runner_->Run({kIpPath, "link", "delete", ifname}, false /* log */);
}

bool Datapath::AddInterfaceToContainer(int ns,
                                       const std::string& src_ifname,
                                       const std::string& dst_ifname,
                                       const std::string& dst_ipv4,
                                       bool fwd_multicast) {
  const std::string pid = base::IntToString(ns);
  return (process_runner_->Run(
              {kIpPath, "link", "set", src_ifname, "netns", pid}) == 0) &&
         (process_runner_->AddInterfaceToContainer(src_ifname, dst_ifname,
                                                   dst_ipv4, kDefaultNetmask,
                                                   fwd_multicast, pid) == 0);
}

bool Datapath::AddLegacyIPv4DNAT(const std::string& ipv4_addr) {
  // Forward "unclaimed" packets to Android to allow inbound connections
  // from devices on the LAN.
  if (process_runner_->Run(
          {kIpTablesPath, "-t", "nat", "-N", "dnat_arc", "-w"}) != 0)
    return false;

  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "dnat_arc", "-j",
                            "DNAT", "--to-destination", ipv4_addr, "-w"}) !=
      0) {
    RemoveLegacyIPv4DNAT();
    return false;
  }

  // This chain is dynamically updated whenever the default interface
  // changes.
  if (process_runner_->Run(
          {kIpTablesPath, "-t", "nat", "-N", "try_arc", "-w"}) != 0) {
    RemoveLegacyIPv4DNAT();
    return false;
  }

  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING",
                            "-m", "socket", "--nowildcard", "-j", "ACCEPT",
                            "-w"}) != 0) {
    RemoveLegacyIPv4DNAT();
    return false;
  }

  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING",
                            "-p", "tcp", "-j", "try_arc", "-w"}) != 0) {
    RemoveLegacyIPv4DNAT();
    return false;
  }

  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING",
                            "-p", "udp", "-j", "try_arc", "-w"}) != 0) {
    RemoveLegacyIPv4DNAT();
    return false;
  }

  return true;
}

void Datapath::RemoveLegacyIPv4DNAT() {
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
}

bool Datapath::AddLegacyIPv4InboundDNAT(const std::string& ifname) {
  return (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "try_arc",
                                "-i", ifname, "-j", "dnat_arc", "-w"}) != 0);
}

void Datapath::RemoveLegacyIPv4InboundDNAT() {
  process_runner_->Run({kIpTablesPath, "-t", "nat", "-F", "try_arc", "-w"});
}

bool Datapath::AddInboundIPv4DNAT(const std::string& ifname,
                                  const std::string& ipv4_addr) {
  // Direct ingress IP traffic to existing sockets.
  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING",
                            "-i", ifname, "-m", "socket", "--nowildcard", "-j",
                            "ACCEPT", "-w"}) != 0)
    return false;

  // Direct ingress TCP & UDP traffic to ARC interface for new connections.
  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING",
                            "-i", ifname, "-p", "tcp", "-j", "DNAT",
                            "--to-destination", ipv4_addr, "-w"}) != 0) {
    RemoveInboundIPv4DNAT(ifname, ipv4_addr);
    return false;
  }
  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "PREROUTING",
                            "-i", ifname, "-p", "udp", "-j", "DNAT",
                            "--to-destination", ipv4_addr, "-w"}) != 0) {
    RemoveInboundIPv4DNAT(ifname, ipv4_addr);
    return false;
  }

  return true;
}

void Datapath::RemoveInboundIPv4DNAT(const std::string& ifname,
                                     const std::string& ipv4_addr) {
  process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-i",
                        ifname, "-p", "udp", "-j", "DNAT", "--to-destination",
                        ipv4_addr, "-w"});
  process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-i",
                        ifname, "-p", "tcp", "-j", "DNAT", "--to-destination",
                        ipv4_addr, "-w"});
  process_runner_->Run({kIpTablesPath, "-t", "nat", "-D", "PREROUTING", "-i",
                        ifname, "-m", "socket", "--nowildcard", "-j", "ACCEPT",
                        "-w"});
}

bool Datapath::AddOutboundIPv4(const std::string& ifname) {
  return process_runner_->Run({kIpTablesPath, "-t", "filter", "-A", "FORWARD",
                               "-o", ifname, "-j", "ACCEPT", "-w"}) == 0;
}

void Datapath::RemoveOutboundIPv4(const std::string& ifname) {
  process_runner_->Run({kIpTablesPath, "-t", "filter", "-D", "FORWARD", "-o",
                        ifname, "-j", "ACCEPT", "-w"});
}

bool Datapath::AddIPv6GatewayRoutes(const std::string& ifname,
                                    const std::string& ipv6_addr,
                                    const std::string& ipv6_router,
                                    int ipv6_prefix_len,
                                    int routing_table) {
  std::string ipv6_addr_cidr =
      ipv6_addr + "/" + std::to_string(ipv6_prefix_len);

  process_runner_->Run(
      {kIpPath, "-6", "addr", "add", ipv6_addr_cidr, "dev", ifname});

  process_runner_->Run({kIpPath, "-6", "route", "add", ipv6_router, "dev",
                        ifname, "table", std::to_string(routing_table)});

  process_runner_->Run({kIpPath, "-6", "route", "add", "default", "via",
                        ipv6_router, "dev", ifname, "table",
                        std::to_string(routing_table)});
  return true;
}

void Datapath::RemoveIPv6GatewayRoutes(const std::string& ifname,
                                       const std::string& ipv6_addr,
                                       const std::string& ipv6_router,
                                       int ipv6_prefix_len,
                                       int routing_table) {
  std::string ipv6_addr_cidr =
      ipv6_addr + "/" + std::to_string(ipv6_prefix_len);

  process_runner_->Run({kIpPath, "-6", "route", "del", "default", "via",
                        ipv6_router, "dev", ifname, "table",
                        std::to_string(routing_table)});
  process_runner_->Run({kIpPath, "-6", "route", "del", ipv6_router, "dev",
                        ifname, "table", std::to_string(routing_table)});
  process_runner_->Run(
      {kIpPath, "-6", "addr", "del", ipv6_addr_cidr, "dev", ifname}, false);
}

bool Datapath::AddIPv6HostRoute(const std::string& ifname,
                                const std::string& ipv6_addr,
                                int ipv6_prefix_len) {
  std::string ipv6_addr_cidr =
      ipv6_addr + "/" + std::to_string(ipv6_prefix_len);

  return process_runner_->Run({kIpPath, "-6", "route", "add", ipv6_addr_cidr,
                               "dev", ifname}) == 0;
}

void Datapath::RemoveIPv6HostRoute(const std::string& ifname,
                                   const std::string& ipv6_addr,
                                   int ipv6_prefix_len) {
  std::string ipv6_addr_cidr =
      ipv6_addr + "/" + std::to_string(ipv6_prefix_len);

  process_runner_->Run(
      {kIpPath, "-6", "route", "del", ipv6_addr_cidr, "dev", ifname});
}

bool Datapath::AddIPv6Neighbor(const std::string& ifname,
                               const std::string& ipv6_addr) {
  return process_runner_->Run({kIpPath, "-6", "neigh", "add", "proxy",
                               ipv6_addr, "dev", ifname}) == 0;
}

void Datapath::RemoveIPv6Neighbor(const std::string& ifname,
                                  const std::string& ipv6_addr) {
  process_runner_->Run(
      {kIpPath, "-6", "neigh", "del", "proxy", ipv6_addr, "dev", ifname});
}

bool Datapath::AddIPv6Forwarding(const std::string& ifname1,
                                 const std::string& ifname2) {
  if (process_runner_->Run({kIp6TablesPath, "-A", "FORWARD", "-i", ifname1,
                            "-o", ifname2, "-j", "ACCEPT", "-w"}) != 0) {
    return false;
  }

  if (process_runner_->Run({kIp6TablesPath, "-A", "FORWARD", "-i", ifname2,
                            "-o", ifname1, "-j", "ACCEPT", "-w"}) != 0) {
    RemoveIPv6Forwarding(ifname1, ifname2);
    return false;
  }

  return true;
}

void Datapath::RemoveIPv6Forwarding(const std::string& ifname1,
                                    const std::string& ifname2) {
  process_runner_->Run({kIp6TablesPath, "-D", "FORWARD", "-i", ifname1, "-o",
                        ifname2, "-j", "ACCEPT", "-w"});

  process_runner_->Run({kIp6TablesPath, "-D", "FORWARD", "-i", ifname2, "-o",
                        ifname1, "-j", "ACCEPT", "-w"});
}

}  // namespace arc_networkd
