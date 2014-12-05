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
const char kIptablesPath[] = "/sbin/iptables";
}

namespace firewalld {

IpTables::IpTables() : IpTables{kIptablesPath} {}

IpTables::IpTables(const std::string& path) : executable_path_{path} {}

bool IpTables::PunchHole(chromeos::ErrorPtr* error,
                                uint16_t in_port,
                                bool* out_success) {
  *out_success = false;

  if (in_port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    *out_success = false;
    return true;
  }

  if (tcp_holes_.find(in_port) != tcp_holes_.end()) {
    // We have already punched a hole for |in_port|.
    // Be idempotent: do nothing and succeed.
    *out_success = true;
    return true;
  }

  LOG(INFO) << "Punching hole for port " << in_port;
  if (!IpTables::AddAllowRule(executable_path_, in_port)) {
    // If the 'iptables' command fails, this method fails.
    LOG(ERROR) << "Calling 'iptables' failed";
    *out_success = false;
    return true;
  }

  // Track the hole we just punched.
  tcp_holes_.insert(in_port);

  *out_success = true;
  return true;
}

bool IpTables::PlugHole(chromeos::ErrorPtr* error,
                               uint16_t in_port,
                               bool* out_success) {
  *out_success = false;

  if (in_port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    *out_success = false;
    return true;
  }

  if (tcp_holes_.find(in_port) == tcp_holes_.end()) {
    // There is no firewall hole for |in_port|.
    // Even though this makes |PlugHole| not idempotent,
    // and Punch/Plug not entirely symmetrical, fail. It might help catch bugs.
    *out_success = false;
    return true;
  }

  LOG(INFO) << "Plugging hole for port " << in_port;
  if (!IpTables::DeleteAllowRule(executable_path_, in_port)) {
    // If the 'iptables' command fails, this method fails.
    LOG(ERROR) << "Calling 'iptables' failed";
    *out_success = false;
    return true;
  }

  // Stop tracking the hole we just plugged.
  tcp_holes_.erase(in_port);

  *out_success = true;
  return true;
}

// static
bool IpTables::AddAllowRule(const std::string& path, uint16_t port) {
  chromeos::ProcessImpl iptables;
  iptables.AddArg(path);
  iptables.AddArg("-A");  // append
  iptables.AddArg("INPUT");
  iptables.AddArg("-p");  // protocol
  iptables.AddArg("tcp");
  iptables.AddArg("--dport");  // destination port
  std::string port_number = base::StringPrintf("%d", port);
  iptables.AddArg(port_number.c_str());
  iptables.AddArg("-j");
  iptables.AddArg("ACCEPT");

  return iptables.Run() == 0;
}

// static
bool IpTables::DeleteAllowRule(const std::string& path, uint16_t port) {
  chromeos::ProcessImpl iptables;
  iptables.AddArg(path);
  iptables.AddArg("-D");  // delete
  iptables.AddArg("INPUT");
  iptables.AddArg("-p");  // protocol
  iptables.AddArg("tcp");
  iptables.AddArg("--dport");  // destination port
  std::string port_number = base::StringPrintf("%d", port);
  iptables.AddArg(port_number.c_str());
  iptables.AddArg("-j");
  iptables.AddArg("ACCEPT");

  return iptables.Run() == 0;
}

}  // namespace firewalld
