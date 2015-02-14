// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/iptables.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/process.h>

namespace {
const char kIpTablesPath[] = "/sbin/iptables";
const char kIp6TablesPath[] = "/sbin/ip6tables";

// Interface names must be shorter than 'IFNAMSIZ' chars.
// See http://man7.org/linux/man-pages/man7/netdevice.7.html
// 'IFNAMSIZ' is 16 in recent kernels.
// See http://lxr.free-electrons.com/source/include/uapi/linux/if.h#L26
const size_t kInterfaceNameSize = 16;

bool IsValidInterfaceName(const std::string& iface) {
  // |iface| should be shorter than |kInterfaceNameSize| chars,
  // and have only alphanumeric characters.
  if (iface.length() >= kInterfaceNameSize) {
    return false;
  }
  for (auto c : iface) {
    if (!std::isalnum(c)) {
      return false;
    }
  }
  return true;
}
}  // namespace

namespace firewalld {

IpTables::IpTables() : IpTables{kIpTablesPath, kIp6TablesPath} {}

IpTables::IpTables(const std::string& ip4_path, const std::string& ip6_path)
    : ip4_exec_path_{ip4_path}, ip6_exec_path_{ip6_path} {}

IpTables::~IpTables() {
  // Plug all holes when destructed.
  PlugAllHoles();
}

bool IpTables::PunchTcpHole(uint16_t in_port, const std::string& in_interface) {
  return PunchHole(in_port, in_interface, &tcp_holes_, kProtocolTcp);
}

bool IpTables::PunchUdpHole(uint16_t in_port, const std::string& in_interface) {
  return PunchHole(in_port, in_interface, &udp_holes_, kProtocolUdp);
}

bool IpTables::PlugTcpHole(uint16_t in_port, const std::string& in_interface) {
  return PlugHole(in_port, in_interface, &tcp_holes_, kProtocolTcp);
}

bool IpTables::PlugUdpHole(uint16_t in_port, const std::string& in_interface) {
  return PlugHole(in_port, in_interface, &udp_holes_, kProtocolUdp);
}

bool IpTables::PunchHole(uint16_t port,
                         const std::string& interface,
                         std::set<Hole>* holes,
                         ProtocolEnum protocol) {
  if (port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    return false;
  }

  if (!IsValidInterfaceName(interface)) {
    LOG(ERROR) << "Invalid interface name '" << interface << "'";
    return false;
  }

  Hole hole = std::make_pair(port, interface);
  if (holes->find(hole) != holes->end()) {
    // We have already punched a hole for |port| on |interface|.
    // Be idempotent: do nothing and succeed.
    return true;
  }

  std::string sprotocol = protocol == kProtocolTcp ? "TCP" : "UDP";
  LOG(INFO) << "Punching hole for " << sprotocol << " port " << port
            << " on interface '" << interface << "'";
  if (!AddAcceptRules(protocol, port, interface)) {
    // If the 'iptables' command fails, this method fails.
    LOG(ERROR) << "Adding ACCEPT rules failed";
    return false;
  }

  // Track the hole we just punched.
  holes->insert(hole);

  return true;
}

bool IpTables::PlugHole(uint16_t port,
                        const std::string& interface,
                        std::set<Hole>* holes,
                        ProtocolEnum protocol) {
  if (port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    return false;
  }

  Hole hole = std::make_pair(port, interface);

  if (holes->find(hole) == holes->end()) {
    // There is no firewall hole for |port| on |interface|.
    // Even though this makes |PlugHole| not idempotent,
    // and Punch/Plug not entirely symmetrical, fail. It might help catch bugs.
    return false;
  }

  std::string sprotocol = protocol == kProtocolTcp ? "TCP" : "UDP";
  LOG(INFO) << "Plugging hole for " << sprotocol << " port " << port
            << " on interface '" << interface << "'";
  if (!DeleteAcceptRules(protocol, port, interface)) {
    // If the 'iptables' command fails, this method fails.
    LOG(ERROR) << "Deleting ACCEPT rules failed";
    return false;
  }

  // Stop tracking the hole we just plugged.
  holes->erase(hole);

  return true;
}

void IpTables::PlugAllHoles() {
  // Copy the container so that we can remove elements from the original.
  // TCP
  std::set<Hole> holes = tcp_holes_;
  for (auto hole : holes) {
    PlugHole(hole.first /* port */, hole.second /* interface */, &tcp_holes_,
             kProtocolTcp);
  }

  // UDP
  holes = udp_holes_;
  for (auto hole : holes) {
    PlugHole(hole.first /* port */, hole.second /* interface */, &udp_holes_,
             kProtocolUdp);
  }

  CHECK(tcp_holes_.size() == 0) << "Failed to plug all TCP holes.";
  CHECK(udp_holes_.size() == 0) << "Failed to plug all UDP holes.";
}

bool IpTables::AddAcceptRules(ProtocolEnum protocol,
                              uint16_t port,
                              const std::string& interface) {
  if (!AddAcceptRule(ip4_exec_path_, protocol, port, interface)) {
    LOG(ERROR) << "Could not add ACCEPT rule using '" << ip4_exec_path_ << "'";
    return false;
  }
  if (!AddAcceptRule(ip6_exec_path_, protocol, port, interface)) {
    LOG(ERROR) << "Could not add ACCEPT rule using '" << ip6_exec_path_ << "'";
    DeleteAcceptRule(ip4_exec_path_, protocol, port, interface);
    return false;
  }
  return true;
}

bool IpTables::DeleteAcceptRules(ProtocolEnum protocol,
                                 uint16_t port,
                                 const std::string& interface) {
  bool ip4_success = DeleteAcceptRule(ip4_exec_path_, protocol, port,
                                      interface);
  bool ip6_success = DeleteAcceptRule(ip6_exec_path_, protocol, port,
                                      interface);
  return ip4_success && ip6_success;
}

bool IpTables::AddAcceptRule(const std::string& executable_path,
                             ProtocolEnum protocol,
                             uint16_t port,
                             const std::string& interface) {
  chromeos::ProcessImpl iptables;
  iptables.AddArg(executable_path);
  iptables.AddArg("-I");  // insert
  iptables.AddArg("INPUT");
  iptables.AddArg("-p");  // protocol
  iptables.AddArg(protocol == kProtocolTcp ? "tcp" : "udp");
  iptables.AddArg("--dport");  // destination port
  iptables.AddArg(std::to_string(port));
  if (!interface.empty()) {
    iptables.AddArg("-i");  // interface
    iptables.AddArg(interface);
  }
  iptables.AddArg("-j");
  iptables.AddArg("ACCEPT");

  return iptables.Run() == 0;
}

bool IpTables::DeleteAcceptRule(const std::string& executable_path,
                                ProtocolEnum protocol,
                                uint16_t port,
                                const std::string& interface) {
  chromeos::ProcessImpl iptables;
  iptables.AddArg(executable_path);
  iptables.AddArg("-D");  // delete
  iptables.AddArg("INPUT");
  iptables.AddArg("-p");  // protocol
  iptables.AddArg(protocol == kProtocolTcp ? "tcp" : "udp");
  iptables.AddArg("--dport");  // destination port
  std::string port_number = base::StringPrintf("%d", port);
  iptables.AddArg(port_number.c_str());
  if (interface != "") {
    iptables.AddArg("-i");  // interface
    iptables.AddArg(interface);
  }
  iptables.AddArg("-j");
  iptables.AddArg("ACCEPT");

  return iptables.Run() == 0;
}

}  // namespace firewalld
