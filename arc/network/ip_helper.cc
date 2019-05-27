// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/ip_helper.h"

#include <ctype.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/posix/unix_domain_socket_linux.h>

namespace arc_networkd {

IpHelper::IpHelper(base::ScopedFD control_fd) : control_watcher_(FROM_HERE) {
  control_fd_ = std::move(control_fd);
}

int IpHelper::OnInit() {
  // Prevent the main process from sending us any signals.
  if (setsid() < 0) {
    PLOG(ERROR) << "Failed to created a new session with setsid: exiting";
    return -1;
  }

  // Handle signals for ARC lifecycle.
  RegisterHandler(SIGUSR1,
                  base::Bind(&IpHelper::OnSignal, base::Unretained(this)));
  RegisterHandler(SIGUSR2,
                  base::Bind(&IpHelper::OnSignal, base::Unretained(this)));

  // This needs to execute after Daemon::OnInit().
  base::MessageLoopForIO::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&IpHelper::InitialSetup, weak_factory_.GetWeakPtr()));

  return Daemon::OnInit();
}

void IpHelper::InitialSetup() {
  // Ensure that the parent is alive before trying to continue the setup.
  char buffer = 0;
  if (!base::UnixDomainSocket::SendMsg(control_fd_.get(), &buffer,
                                       sizeof(buffer), std::vector<int>())) {
    LOG(ERROR) << "Aborting setup flow because the parent died";
    Quit();
    return;
  }

  arc_helper_ = ArcHelper::New();
  if (!arc_helper_) {
    LOG(ERROR) << "Aborting setup flow";
    Quit();
    return;
  }

  MessageLoopForIO::current()->WatchFileDescriptor(control_fd_.get(), true,
                                                   MessageLoopForIO::WATCH_READ,
                                                   &control_watcher_, this);
}

bool IpHelper::OnSignal(const struct signalfd_siginfo& info) {
  if (info.ssi_signo == SIGUSR1) {
    arc_helper_->Start();
  } else if (info.ssi_signo == SIGUSR2) {
    arc_helper_->Stop();
  }

  // Stay registered.
  return false;
}

void IpHelper::OnFileCanReadWithoutBlocking(int fd) {
  CHECK_EQ(fd, control_fd_.get());

  char buffer[1024];
  std::vector<base::ScopedFD> fds{};
  ssize_t len =
      base::UnixDomainSocket::RecvMsg(fd, buffer, sizeof(buffer), &fds);

  // Exit whenever read fails or fd is closed.
  if (len <= 0) {
    PLOG(WARNING) << "Read failed: exiting";
    control_watcher_.StopWatchingFileDescriptor();
    Quit();
    return;
  }

  if (!pending_command_.ParseFromArray(buffer, len)) {
    LOG(ERROR) << "Error parsing protobuf";
    return;
  }

  arc_helper_->HandleCommand(pending_command_);
  pending_command_.Clear();
}

}  // namespace arc_networkd
