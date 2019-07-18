// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/datapath.h"

namespace arc_networkd {
namespace {
constexpr char kDefaultNetmask[] = "255.255.255.252";
}  // namespace

Datapath::Datapath(MinijailedProcessRunner* process_runner)
    : process_runner_(process_runner) {
  CHECK(process_runner_);
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

bool Datapath::AddLegacyIPv4DNAT(const std::string& ipv4_addr) {
  // Forward "unclaimed" packets to Android to allow inbound connections
  // from devices on the LAN.
  if (process_runner_->Run(
          {kIpTablesPath, "-t", "nat", "-N", "dnat_arc", "-w"}) != 0)
    return false;

  if (process_runner_->Run({kIpTablesPath, "-t", "nat", "-A", "dnat_arc", "-j",
                            "DNAT", "--to-destination", ipv4_addr, "-w"}) !=
      0) {
    //
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

}  // namespace arc_networkd
