// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/helper_process.h"

#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/process/launch.h>
#include <brillo/syslog_logging.h>

namespace arc_networkd {

void HelperProcess::Start(int argc, char* argv[], const std::string& fd_arg) {
  int control[2];

  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, control) != 0) {
    PLOG(FATAL) << "socketpair failed";
  }

  base::ScopedFD control_fd(control[0]);
  msg_dispatcher_ =
      std::make_unique<MessageDispatcher>(std::move(control_fd), false);
  const int subprocess_fd = control[1];

  CHECK_GE(argc, 1);
  std::vector<std::string> child_argv;
  for (int i = 0; i < argc; i++) {
    child_argv.push_back(argv[i]);
  }
  child_argv.push_back(fd_arg + "=" + std::to_string(subprocess_fd));

  base::FileHandleMappingVector fd_mapping;
  fd_mapping.push_back({subprocess_fd, subprocess_fd});

  base::LaunchOptions options;
  // TODO(fqj): change to std::move(fd_mapping)
  options.fds_to_remap = FDS_TO_REMAP(fd_mapping);

  base::Process p = base::LaunchProcess(child_argv, options);
  CHECK(p.IsValid());
  pid_ = p.Pid();
}

void HelperProcess::SendMessage(
    const google::protobuf::MessageLite& proto) const {
  msg_dispatcher_->SendMessage(proto);
}

void HelperProcess::Listen() {
  msg_dispatcher_->Start();
}

void HelperProcess::RegisterDeviceMessageHandler(
    const base::Callback<void(const DeviceMessage&)>& handler) {
  msg_dispatcher_->RegisterDeviceMessageHandler(handler);
}

}  // namespace arc_networkd
