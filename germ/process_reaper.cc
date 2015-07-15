// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/process_reaper.h"

#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <base/bind.h>
#include <base/posix/eintr_wrapper.h>
#include <chromeos/asynchronous_signal_handler.h>
#include <chromeos/daemons/daemon.h>

namespace germ {

ProcessReaper::ProcessReaper() {}
ProcessReaper::~ProcessReaper() {}

void ProcessReaper::RegisterWithAsyncHandler(
    chromeos::AsynchronousSignalHandler* async_signal_handler) {
  async_signal_handler->RegisterHandler(
      SIGCHLD,
      base::Bind(&ProcessReaper::HandleSIGCHLD, base::Unretained(this)));
}

void ProcessReaper::RegisterWithDaemon(chromeos::Daemon* daemon) {
  daemon->RegisterHandler(SIGCHLD, base::Bind(&ProcessReaper::HandleSIGCHLD,
                                              base::Unretained(this)));
}

bool ProcessReaper::HandleSIGCHLD(const struct signalfd_siginfo& sigfd_info) {
  // One SIGCHLD may correspond to multiple terminated children, so ignore
  // sigfd_info and reap any available children.
  while (true) {
    siginfo_t info;
    info.si_pid = 0;
    int rc = HANDLE_EINTR(waitid(P_ALL, 0, &info, WNOHANG | WEXITED));

    if (rc == -1) {
      if (errno != ECHILD) {
        PLOG(ERROR) << "waitid failed";
      }
      break;
    }

    if (info.si_pid == 0) {
      break;
    }

    HandleReapedChild(info);
  }

  // Return false to indicate that our handler should not be uninstalled.
  return false;
}

void ProcessReaper::HandleReapedChild(const siginfo_t& info) {
  LOG(INFO) << "Process " << info.si_pid << " terminated with status "
            << info.si_status << " (code = " << info.si_code << ")";
}

}  // namespace germ
