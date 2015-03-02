// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/iptables.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/process.h>

namespace {
const char kIpTablesPath[] = "/sbin/iptables";
const char kIp6TablesPath[] = "/sbin/ip6tables";

const char kIpPath[] = "/bin/ip";

// Interface names must be shorter than 'IFNAMSIZ' chars.
// See http://man7.org/linux/man-pages/man7/netdevice.7.html
// 'IFNAMSIZ' is 16 in recent kernels.
// See http://lxr.free-electrons.com/source/include/uapi/linux/if.h#L26
const size_t kInterfaceNameSize = 16;

const char kMarkForUserTraffic[] = "1";

const char kTableIdForUserTraffic[] = "1";

bool IsValidInterfaceName(const std::string& iface) {
  // |iface| should be shorter than |kInterfaceNameSize| chars and have only
  // alphanumeric characters (embedded hypens are also permitted).
  if (iface.length() >= kInterfaceNameSize) {
    return false;
  }
  if (StartsWithASCII(iface, "-", true /* case_sensitive */) ||
      EndsWith(iface, "-", true /* case_sensitive */)) {
    return false;
  }
  for (auto c : iface) {
    if (!std::isalnum(c) && (c != '-')) {
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

bool IpTables::RequestVpnSetup(const std::vector<std::string>& usernames,
                               const std::string& interface) {
  return ApplyVpnSetup(usernames, interface, true /* add */);
}

bool IpTables::RemoveVpnSetup(const std::vector<std::string>& usernames,
                              const std::string& interface) {
  return ApplyVpnSetup(usernames, interface, false /* delete */);
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

bool IpTables::ApplyVpnSetup(const std::vector<std::string>& usernames,
                             const std::string& interface,
                             bool add) {
  bool return_value = true;

  if (!ApplyRuleForUserTraffic(add)) {
    LOG(ERROR) << (add ? "Adding" : "Removing")
               << " rule for user traffic failed";
    if (add)
      return false;
    return_value = false;
  }

  if (!ApplyMasquerade(interface, add)) {
    LOG(ERROR) << (add ? "Adding" : "Removing")
               << " masquerade failed for interface "
               << interface;
    if (add) {
      ApplyRuleForUserTraffic(false);
      return false;
    }
    return_value = false;
  }

  std::vector<std::string> added_usernames;
  for (const auto& username : usernames) {
    if (!ApplyMarkForUserTraffic(username, add)) {
      LOG(ERROR) << (add ? "Adding" : "Removing")
                 << " mark failed for user "
                 << username;
      if (add) {
        ApplyVpnSetup(added_usernames, interface, false);
        return false;
      }
      return_value = false;
    }
    if (add) {
      added_usernames.push_back(username);
    }
  }

  return return_value;
}

bool IpTables::ApplyMasquerade(const std::string& interface, bool add) {
  chromeos::ProcessImpl iptables;
  iptables.AddArg(ip4_exec_path_);
  iptables.AddArg("-t");  // table
  iptables.AddArg("nat");
  iptables.AddArg(add ? "-A" : "-D");  // rule
  iptables.AddArg("POSTROUTING");
  iptables.AddArg("-o");  // output interface
  iptables.AddArg(interface);
  iptables.AddArg("-j");
  iptables.AddArg("MASQUERADE");

  return iptables.Run() == 0;
}

bool IpTables::ApplyMarkForUserTraffic(const std::string& user_name,
                                       bool add) {
  chromeos::ProcessImpl iptables;
  iptables.AddArg(ip4_exec_path_);
  iptables.AddArg("-t");  // table
  iptables.AddArg("mangle");
  iptables.AddArg(add ? "-A" : "-D");  // rule
  iptables.AddArg("OUTPUT");
  iptables.AddArg("-m");
  iptables.AddArg("owner");
  iptables.AddArg("--uid-owner");
  iptables.AddArg(user_name);
  iptables.AddArg("-j");
  iptables.AddArg("MARK");
  iptables.AddArg("--set-mark");
  iptables.AddArg(kMarkForUserTraffic);

  return iptables.Run() == 0;
}

bool IpTables::ApplyRuleForUserTraffic(bool add) {
  chromeos::ProcessImpl ip;
  ip.AddArg(kIpPath);
  ip.AddArg("rule");
  ip.AddArg(add ? "add" : "delete");
  ip.AddArg("fwmark");
  ip.AddArg(kMarkForUserTraffic);
  ip.AddArg("table");
  ip.AddArg(kTableIdForUserTraffic);

  return ip.Run() == 0;
}

}  // namespace firewalld
