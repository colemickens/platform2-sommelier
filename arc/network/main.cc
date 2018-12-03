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

int main(int argc, char* argv[]) {
  DEFINE_bool(log_to_stderr, false, "Log to both syslog and stderr");
  DEFINE_int32(
      ip_helper_fd, -1,
      "Control socket for starting an IpHelper subprocess. Used internally.");

  brillo::FlagHelper::Init(argc, argv, "ARC network daemon");

  int flags = brillo::kLogToSyslog | brillo::kLogHeader;
  if (FLAGS_log_to_stderr)
    flags |= brillo::kLogToStderr;
  brillo::InitLog(flags);

  LOG(INFO) << "Starting arc-networkd "
            << (FLAGS_ip_helper_fd >= 0 ? "helper" : "manager");

  if (FLAGS_ip_helper_fd >= 0) {
    base::ScopedFD fd(FLAGS_ip_helper_fd);
    arc_networkd::IpHelper ip_helper{std::move(fd)};
    return ip_helper.Run();
  }

  auto ip_helper = std::make_unique<arc_networkd::HelperProcess>();
  ip_helper->Start(argc, argv, "--ip_helper_fd");

  arc_networkd::Manager manager{std::move(ip_helper)};
  return manager.Run();
}
