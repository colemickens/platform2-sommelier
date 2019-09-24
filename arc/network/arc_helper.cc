// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_helper.h"

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>

#include "arc/network/minijailed_process_runner.h"

namespace arc_networkd {

// static
std::unique_ptr<ArcHelper> ArcHelper::New() {
  // Load networking modules needed by Android that are not compiled in the
  // kernel. Android does not allow auto-loading of kernel modules.
  auto process_runner = std::make_unique<MinijailedProcessRunner>();

  // These must succeed.
  if (process_runner->ModprobeAll({
          // The netfilter modules needed by netd for iptables commands.
          "ip6table_filter",
          "ip6t_ipv6header",
          "ip6t_REJECT",
          // The xfrm modules needed for Android's ipsec APIs.
          "xfrm4_mode_transport",
          "xfrm4_mode_tunnel",
          "xfrm6_mode_transport",
          "xfrm6_mode_tunnel",
          // The ipsec modules for AH and ESP encryption for ipv6.
          "ah6",
          "esp6",
      }) != 0) {
    LOG(ERROR) << "One or more required kernel modules failed to load.";
    return nullptr;
  }

  // Optional modules.
  if (process_runner->ModprobeAll({
          // This module is not available in kernels < 3.18
          "nf_reject_ipv6",
          // These modules are needed for supporting Chrome traffic on Android
          // VPN which uses Android's NAT feature. Android NAT sets up iptables
          // rules that use these conntrack modules for FTP/TFTP.
          "nf_nat_ftp",
          "nf_nat_tftp",
      }) != 0) {
    LOG(INFO) << "One or more optional kernel modules failed to load.";
  }

  return base::WrapUnique(new ArcHelper);
}

void ArcHelper::Start(pid_t pid) {
  LOG(INFO) << "Container starting [" << pid << "]";
  pid_ = pid;
  CHECK_NE(pid_, 0);
}

void ArcHelper::Stop(pid_t pid) {
  if (pid != pid_) {
    LOG(DFATAL) << "Mismatched pid: " << pid;
    return;
  }
  LOG(INFO) << "Container stopping [" << pid_ << "]";

  pid_ = 0;
}

void ArcHelper::AddDevice(const std::string& ifname,
                          const DeviceConfig& config) {
  LOG(INFO) << "Adding device " << ifname;
}

void ArcHelper::RemoveDevice(const std::string& ifname) {
  LOG(INFO) << "Removing device " << ifname;
}

void ArcHelper::HandleCommand(const DeviceMessage& cmd) {}

}  // namespace arc_networkd
