// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_exit_dispatcher.h"

#include <algorithm>

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <base/bind.h>
#include <base/logging.h>
#include <brillo/asynchronous_signal_handler.h>

#include "login_manager/child_job.h"
#include "login_manager/job_manager.h"

namespace login_manager {

ChildExitDispatcher::ChildExitDispatcher(
    brillo::AsynchronousSignalHandler* signal_handler,
    const std::vector<JobManagerInterface*>& managers)
    : signal_handler_(signal_handler), managers_(managers) {
  signal_handler_->RegisterHandler(
      SIGCHLD,
      base::Bind(&ChildExitDispatcher::OnSigChld, base::Unretained(this)));
}

ChildExitDispatcher::~ChildExitDispatcher() {
  signal_handler_->UnregisterHandler(SIGCHLD);
}

bool ChildExitDispatcher::OnSigChld(const struct signalfd_siginfo& sig_info) {
  DCHECK_EQ(sig_info.ssi_signo, SIGCHLD) << "Wrong signal!";
  if (sig_info.ssi_code == CLD_STOPPED || sig_info.ssi_code == CLD_CONTINUED) {
    return false;
  }
  siginfo_t info;
  // Reap all terminated children.
  while (true) {
    memset(&info, 0, sizeof(info));
    int result = waitid(P_ALL, 0, &info, WEXITED | WNOHANG);
    if (result != 0) {
      if (errno != ECHILD)
        PLOG(FATAL) << "waitid failed";
      break;
    }
    if (info.si_pid == 0)
      break;
    Dispatch(info);
  }
  // Continue listening to SIGCHLD
  return false;
}

void ChildExitDispatcher::Dispatch(const siginfo_t& info) {
  // Find the manager whose child has exited.
  pid_t pid = info.si_pid;
  auto iter = std::find_if(managers_.begin(), managers_.end(),
                           [pid](JobManagerInterface* manager) {
                             return manager->IsManagedJob(pid);
                           });
  if (iter == managers_.end()) {
    DLOG(INFO) << info.si_pid << " is not a managed job.";
    return;
  }

  LOG(INFO) << "Handling " << info.si_pid << " exit.";
  if (info.si_code == CLD_EXITED) {
    LOG_IF(ERROR, info.si_status != 0)
        << "  Exited with exit code " << info.si_status;
    CHECK_NE(info.si_status, ChildJobInterface::kCantSetUid);
    CHECK_NE(info.si_status, ChildJobInterface::kCantSetEnv);
    CHECK_NE(info.si_status, ChildJobInterface::kCantExec);
  } else {
    LOG(ERROR) << "  Exited with signal " << info.si_status;
  }
  (*iter)->HandleExit(info);
}

}  // namespace login_manager
