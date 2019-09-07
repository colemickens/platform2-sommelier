// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/firewall.h"

#include <linux/capability.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/minijail/minijail.h>

namespace {

// Interface names must be shorter than 'IFNAMSIZ' chars.
// See http://man7.org/linux/man-pages/man7/netdevice.7.html
// 'IFNAMSIZ' is 16 in recent kernels.
// See https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/if.h?h=v4.14#n33
constexpr size_t kInterfaceNameSize = 16;

// Interface names are passed directly to the 'iptables' command. Rather than
// auditing 'iptables' source code to see how it handles malformed names,
// do some sanitization on the names beforehand.
bool IsValidInterfaceName(const std::string& iface) {
  // |iface| should be shorter than |kInterfaceNameSize| chars and have only
  // alphanumeric characters (embedded hypens and periods are also permitted).
  if (iface.length() >= kInterfaceNameSize) {
    return false;
  }
  if (base::StartsWith(iface, "-", base::CompareCase::SENSITIVE) ||
      base::EndsWith(iface, "-", base::CompareCase::SENSITIVE) ||
      base::StartsWith(iface, ".", base::CompareCase::SENSITIVE) ||
      base::EndsWith(iface, ".", base::CompareCase::SENSITIVE)) {
    return false;
  }
  for (auto c : iface) {
    if (!std::isalnum(c) && (c != '-') && (c != '.')) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace permission_broker {

const char kIpTablesPath[] = "/sbin/iptables";
const char kIp6TablesPath[] = "/sbin/ip6tables";

bool Firewall::AddAcceptRules(ProtocolEnum protocol,
                              uint16_t port,
                              const std::string& interface) {
  if (port == 0U) {
    LOG(ERROR) << "Port 0 is not a valid port";
    return false;
  }

  if (!IsValidInterfaceName(interface)) {
    LOG(ERROR) << "Invalid interface name '" << interface << "'";
    return false;
  }

  if (!AddAcceptRule(kIpTablesPath, protocol, port, interface)) {
    LOG(ERROR) << "Could not add ACCEPT rule using '" << kIpTablesPath << "'";
    return false;
  }

  if (!AddAcceptRule(kIp6TablesPath, protocol, port, interface)) {
    LOG(ERROR) << "Could not add ACCEPT rule using '" << kIp6TablesPath
               << "', aborting operation";
    DeleteAcceptRule(kIpTablesPath, protocol, port, interface);
    return false;
  }

  return true;
}

bool Firewall::DeleteAcceptRules(ProtocolEnum protocol,
                                 uint16_t port,
                                 const std::string& interface) {
  if (port == 0U) {
    LOG(ERROR) << "Port 0 is not a valid port";
    return false;
  }

  if (!IsValidInterfaceName(interface)) {
    LOG(ERROR) << "Invalid interface name '" << interface << "'";
    return false;
  }

  bool ip4_success = DeleteAcceptRule(kIpTablesPath, protocol, port,
                                      interface);
  bool ip6_success = DeleteAcceptRule(kIp6TablesPath, protocol,
                                      port, interface);
  return ip4_success && ip6_success;
}

bool Firewall::AddLoopbackLockdownRules(ProtocolEnum protocol, uint16_t port) {
  if (port == 0U) {
    LOG(ERROR) << "Port 0 is not a valid port";
    return false;
  }

  if (!AddLoopbackLockdownRule(kIpTablesPath, protocol, port)) {
    LOG(ERROR) << "Could not add loopback REJECT rule using '" << kIpTablesPath
               << "'";
    return false;
  }

  if (!AddLoopbackLockdownRule(kIp6TablesPath, protocol, port)) {
    LOG(ERROR) << "Could not add loopback REJECT rule using '" << kIp6TablesPath
               << "', aborting operation";
    DeleteLoopbackLockdownRule(kIpTablesPath, protocol, port);
    return false;
  }

  return true;
}

bool Firewall::DeleteLoopbackLockdownRules(ProtocolEnum protocol,
                                           uint16_t port) {
  if (port == 0U) {
    LOG(ERROR) << "Port 0 is not a valid port";
    return false;
  }

  bool ip4_success = DeleteLoopbackLockdownRule(kIpTablesPath, protocol, port);
  bool ip6_success = DeleteLoopbackLockdownRule(kIp6TablesPath, protocol, port);
  return ip4_success && ip6_success;
}

bool Firewall::AddAcceptRule(const std::string& executable_path,
                             ProtocolEnum protocol,
                             uint16_t port,
                             const std::string& interface) {
  std::vector<std::string> argv{executable_path,
                                "-I",  // insert
                                "INPUT",
                                "-p",  // protocol
                                protocol == kProtocolTcp ? "tcp" : "udp",
                                "--dport",  // destination port
                                std::to_string(port)};
  if (!interface.empty()) {
    argv.push_back("-i");  // interface
    argv.push_back(interface);
  }
  argv.push_back("-j");
  argv.push_back("ACCEPT");
  argv.push_back("-w");  // Wait for xtables lock.

  return RunInMinijail(argv) == 0;
}

bool Firewall::DeleteAcceptRule(const std::string& executable_path,
                                ProtocolEnum protocol,
                                uint16_t port,
                                const std::string& interface) {
  std::vector<std::string> argv{executable_path,
                                "-D",  // delete
                                "INPUT",
                                "-p",  // protocol
                                protocol == kProtocolTcp ? "tcp" : "udp",
                                "--dport",  // destination port
                                std::to_string(port)};
  if (!interface.empty()) {
    argv.push_back("-i");  // interface
    argv.push_back(interface);
  }
  argv.push_back("-j");
  argv.push_back("ACCEPT");
  argv.push_back("-w");  // Wait for xtables lock.

  return RunInMinijail(argv) == 0;
}

bool Firewall::AddLoopbackLockdownRule(const std::string& executable_path,
                                       ProtocolEnum protocol,
                                       uint16_t port) {
  std::vector<std::string> argv{
      executable_path,
      "-I",  // insert
      "OUTPUT",
      "-p",  // protocol
      protocol == kProtocolTcp ? "tcp" : "udp",
      "--dport",  // destination port
      std::to_string(port),
      "-o",  // output interface
      "lo",
      "-m",  // match extension
      "owner",
      "!",
      "--uid-owner",
      "chronos",
      "-j",
      "REJECT",
      "-w",  // Wait for xtables lock.
  };

  return RunInMinijail(argv) == 0;
}

bool Firewall::DeleteLoopbackLockdownRule(const std::string& executable_path,
                                          ProtocolEnum protocol,
                                          uint16_t port) {
  std::vector<std::string> argv{
      executable_path,
      "-D",  // delete
      "OUTPUT",
      "-p",  // protocol
      protocol == kProtocolTcp ? "tcp" : "udp",
      "--dport",  // destination port
      std::to_string(port),
      "-o",  // output interface
      "lo",
      "-m",  // match extension
      "owner",
      "!",
      "--uid-owner",
      "chronos",
      "-j",
      "REJECT",
      "-w",  // Wait for xtables lock.
  };

  return RunInMinijail(argv) == 0;
}

int Firewall::RunInMinijail(const std::vector<std::string>& argv) {
  brillo::Minijail* m = brillo::Minijail::GetInstance();
  minijail* jail = m->New();

  std::vector<char*> args;
  for (const auto& arg : argv) {
    args.push_back(const_cast<char*>(arg.c_str()));
  }
  args.push_back(nullptr);

  int status;
  return m->RunSyncAndDestroy(jail, args, &status) ? status : -1;
}

}  // namespace permission_broker
