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

using IpTablesCallback = base::Callback<bool(const std::string&, bool)>;

const char kIPv4[] = "IPv4";
const char kIPv6[] = "IPv6";

// Interface names must be shorter than 'IFNAMSIZ' chars.
// See http://man7.org/linux/man-pages/man7/netdevice.7.html
// 'IFNAMSIZ' is 16 in recent kernels.
// See https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/if.h?h=v4.14#n33
constexpr size_t kInterfaceNameSize = 16;

const char kMarkForUserTraffic[] = "1";

const char kTableIdForUserTraffic[] = "1";

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

bool RunForAllArguments(const IpTablesCallback& iptables_cmd,
                        const std::vector<std::string>& arguments,
                        bool add) {
  bool success = true;
  for (const auto& argument : arguments) {
    if (!iptables_cmd.Run(argument, add)) {
      // On failure, only abort if rules are being added.
      // If removing a rule fails, attempt the remaining removals but still
      // return 'false'.
      success = false;
      if (add) {
        break;
      }
    }
  }
  return success;
}

}  // namespace

namespace permission_broker {

const char kIpTablesPath[] = "/sbin/iptables";
const char kIp6TablesPath[] = "/sbin/ip6tables";
const char kIpPath[] = "/bin/ip";

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

  if (AddAcceptRule(kIp6TablesPath, protocol, port, interface)) {
    // This worked, record this fact and insist that it works thereafter.
    ip6_enabled_ = true;
  } else if (ip6_enabled_) {
    // It's supposed to work, fail.
    LOG(ERROR) << "Could not add ACCEPT rule using '" << kIp6TablesPath
               << "', aborting operation";
    DeleteAcceptRule(kIpTablesPath, protocol, port, interface);
    return false;
  } else {
    // It never worked, just ignore it.
    LOG(WARNING) << "Could not add ACCEPT rule using '" << kIp6TablesPath
                 << "', ignoring";
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
  bool ip6_success = !ip6_enabled_ || DeleteAcceptRule(kIp6TablesPath, protocol,
                                                       port, interface);
  return ip4_success && ip6_success;
}

bool Firewall::ApplyVpnSetup(const std::vector<std::string>& usernames,
                             const std::string& interface,
                             bool add) {
  bool success = true;
  std::vector<std::string> added_usernames;

  if (!ApplyRuleForUserTraffic(add)) {
    if (add) {
      ApplyRuleForUserTraffic(false /* remove */);
      return false;
    }
    success = false;
  }

  if (!ApplyMasquerade(interface, add)) {
    if (add) {
      ApplyVpnSetup(added_usernames, interface, false /* remove */);
      return false;
    }
    success = false;
  }

  for (const auto& username : usernames) {
    if (!ApplyMarkForUserTraffic(username, add)) {
      if (add) {
        ApplyVpnSetup(added_usernames, interface, false /* remove */);
        return false;
      }
      success = false;
    }
    if (add) {
      added_usernames.push_back(username);
    }
  }

  return success;
}

bool Firewall::ApplyMasquerade(const std::string& interface, bool add) {
  const IpTablesCallback apply_masquerade =
      base::Bind(&Firewall::ApplyMasqueradeWithExecutable,
                 base::Unretained(this),
                 interface);

  return RunForAllArguments(
      apply_masquerade, {kIpTablesPath, kIp6TablesPath}, add);
}

bool Firewall::ApplyMarkForUserTraffic(const std::string& username, bool add) {
  const IpTablesCallback apply_mark =
      base::Bind(&Firewall::ApplyMarkForUserTrafficWithExecutable,
                 base::Unretained(this),
                 username);

  return RunForAllArguments(apply_mark, {kIpTablesPath, kIp6TablesPath}, add);
}

bool Firewall::ApplyRuleForUserTraffic(bool add) {
  const IpTablesCallback apply_rule = base::Bind(
      &Firewall::ApplyRuleForUserTrafficWithVersion, base::Unretained(this));

  return RunForAllArguments(apply_rule, {kIPv4, kIPv6}, add);
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

bool Firewall::ApplyMasqueradeWithExecutable(const std::string& interface,
                                             const std::string& executable_path,
                                             bool add) {
  bool success = true;
  std::vector<std::string> argv{
      executable_path,
      "-t",  // table
      "nat",
      add ? "-A" : "-D",  // rule
      "POSTROUTING",
      "-o",  // output interface
      interface,
      "-j",
      "MASQUERADE",
      "-w"  // Wait for xtables lock
  };

  if (RunInMinijail(argv)) {
    LOG(ERROR) << (add ? "Adding" : "Removing")
               << " masquerade failed for interface " << interface
               << " using '" << executable_path << "'";
    success = false;
    if (add) {
      return false;
    }
  }

  argv.assign({
      executable_path,
      "-t",  // table
      "mangle",
      add ? "-A" : "-D",  // rule
      "POSTROUTING",
      "-p",
      "tcp",
      "-o",  // output interface
      interface,
      "--tcp-flags",
      "SYN,RST",
      "SYN",
      "-j",
      "TCPMSS",
      "--clamp-mss-to-pmtu",
      "-w",  // Wait for xtables lock
  });

  if (RunInMinijail(argv)) {
    LOG(ERROR) << (add ? "Adding" : "Removing")
               << " tcpmss rule failed for interface " << interface
               << " using '" << executable_path << "'";
    success = false;
  }

  return success;
}

bool Firewall::ApplyMarkForUserTrafficWithExecutable(
    const std::string& username, const std::string& executable_path, bool add) {
  std::vector<std::string> argv{
      executable_path,
      "-t",  // table
      "mangle",
      add ? "-A" : "-D",  // rule
      "OUTPUT",
      "-m",
      "owner",
      "--uid-owner",
      username,
      "-j",
      "MARK",
      "--set-mark",
      kMarkForUserTraffic,
      "-w"  // Wait for xtables lock
  };

  // Use CAP_NET_ADMIN|CAP_NET_RAW.
  bool success = RunInMinijail(argv) == 0;

  if (!success) {
      LOG(ERROR) << (add ? "Adding" : "Removing")
                 << " mark failed for user " << username
                 << " using '" << kIpTablesPath << "'";
  }
  return success;
}

bool Firewall::ApplyRuleForUserTrafficWithVersion(const std::string& ip_version,
                                                  bool add) {
  std::vector<std::string> argv{kIpPath};
  if (ip_version == kIPv6)
    argv.push_back("-6");
  argv.push_back("rule");
  argv.push_back(add ? "add" : "delete");
  argv.push_back("fwmark");
  argv.push_back(kMarkForUserTraffic);
  argv.push_back("table");
  argv.push_back(kTableIdForUserTraffic);

  bool success = RunInMinijail(argv) == 0;

  if (!success) {
    LOG(ERROR) << (add ? "Adding" : "Removing")
               << " rule for user traffic failed"
               << " using '" << kIpPath << "'";
  }

  return success;
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
