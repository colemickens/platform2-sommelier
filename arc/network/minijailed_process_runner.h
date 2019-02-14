// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MINIJAILED_PROCESS_RUNNER_H_
#define ARC_NETWORK_MINIJAILED_PROCESS_RUNNER_H_

#include <string>
#include <vector>

#include <brillo/minijail/minijail.h>

namespace arc_networkd {

// These match what is used in iptables.cc in firewalld.
const char kBrctlPath[] = "/sbin/brctl";
const char kIfConfigPath[] = "/bin/ifconfig";
const char kIpPath[] = "/bin/ip";
const char kIpTablesPath[] = "/sbin/iptables";
const char kIp6TablesPath[] = "/sbin/ip6tables";

// Enforces the expected processes are run with the correct privileges.
class MinijailedProcessRunner {
 public:
  // Ownership of |mj| is not assumed and must be managed by the caller.
  // If |mj| is null, the default instance will be used.
  explicit MinijailedProcessRunner(brillo::Minijail* mj = nullptr);
  virtual ~MinijailedProcessRunner() = default;

  // Runs a process (argv[0]) with optional arguments (argv[1]...)
  // in a minijail as an unprivileged user with CAP_NET_ADMIN and
  // CAP_NET_RAW capabilities.
  virtual int Run(const std::vector<std::string>& argv,
                  bool log_failures = true);

  // Moves interface |host_ifname| into the container designated by |con_pid|
  // as interface |con_ifname| and assigns it's |con_ipv4_addr|. This method
  // does NOT bring up the interface.
  virtual int AddInterfaceToContainer(const std::string& host_ifname,
                                      const std::string& con_ifname,
                                      const std::string& con_ipv4,
                                      const std::string& con_nmask,
                                      bool enable_multicast,
                                      const std::string& con_pid);

  // Writes out a file that the ARC boot process uses to discover when
  // the host networking is ready.
  virtual int WriteSentinelToContainer(const std::string& con_pid);

 private:
  brillo::Minijail* mj_;

  DISALLOW_COPY_AND_ASSIGN(MinijailedProcessRunner);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MINIJAILED_PROCESS_RUNNER_H_
