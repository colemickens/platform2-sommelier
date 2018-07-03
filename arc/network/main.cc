// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <memory>
#include <utility>

#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "arc/network/helper_process.h"
#include "arc/network/ip_helper.h"
#include "arc/network/manager.h"
#include "arc/network/options.h"

int main(int argc, char* argv[]) {
  DEFINE_bool(log_to_stderr, false, "Log to both syslog and stderr");
  DEFINE_string(internal_interface, "arcbr0",
                "Name of the host interface that connects to the guest");
  DEFINE_string(mdns_ip, "100.115.92.2",
                "Guest IP to replace with the LAN IP in mDNS responses");
  DEFINE_string(container_interface, "arc0",
                "Name of the guest interface that connects to the host");
  DEFINE_int32(con_netns, 0, "Container's network namespace (PID)");
  DEFINE_int32(
      ip_helper_fd, -1,
      "Control socket for starting an IpHelper subprocess. Used internally.");

  brillo::FlagHelper::Init(argc, argv, "ARC network daemon");

  int flags = brillo::kLogToSyslog | brillo::kLogHeader;
  if (FLAGS_log_to_stderr)
    flags |= brillo::kLogToStderr;
  brillo::InitLog(flags);

  arc_networkd::Options opt;
  opt.int_ifname = FLAGS_internal_interface;
  opt.mdns_ipaddr = FLAGS_mdns_ip;
  opt.con_ifname = FLAGS_container_interface;
  opt.con_netns = FLAGS_con_netns;

  LOG(INFO) << "Starting arc-networkd "
            << (FLAGS_ip_helper_fd >= 0 ? "helper" : "manager")
            << " with Option "
            << "{ netns=" << opt.con_netns
            << ", host ifname=" << opt.int_ifname
            << ", guest ifname=" << opt.con_ifname
            << ", mdns ip=" << opt.mdns_ipaddr
            << " }";

  if (FLAGS_ip_helper_fd >= 0) {
    base::ScopedFD fd(FLAGS_ip_helper_fd);
    arc_networkd::IpHelper ip_helper{opt, std::move(fd)};
    return ip_helper.Run();
  } else {
    std::unique_ptr<arc_networkd::HelperProcess> ip_helper(
        new arc_networkd::HelperProcess());
    ip_helper->Start(argc, argv, "--ip_helper_fd");

    arc_networkd::Manager manager{opt, std::move(ip_helper)};
    return manager.Run();
  }
}
