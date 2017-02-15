// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/jail_helper.h"

#include <base/memory/ptr_util.h>

#include "authpolicy/platform_helper.h"
#include "authpolicy/process_executor.h"

namespace authpolicy {

JailHelper::JailHelper(const PathService* path_service)
    : paths_(path_service) {}

bool JailHelper::SetupJailAndRun(ProcessExecutor* cmd,
                                 Path seccomp_path_key,
                                 TimerType timer_type) const {
  // Limit the system calls that the process can do.
  DCHECK(cmd);
  if (!disable_seccomp_filters_) {
    if (log_seccomp_filters_)
      cmd->LogSeccompFilterFailures();
    cmd->SetSeccompFilter(paths_->Get(seccomp_path_key));
  }

  // Required since we don't have the caps to wipe supplementary groups.
  cmd->KeepSupplementaryGroups();

  // Allows us to drop setgroups, setresgid and setresuid from seccomp filters.
  cmd->SetNoNewPrivs();

  // Execute as authpolicyd exec user. Don't use minijail to switch user. This
  // would force us to run without preload library since saved UIDs are wiped by
  // execve and the executed code wouldn't be able to switch user. Running with
  // preload library has two main advantages:
  //   1) Tighter seccomp filters, no need to allow execve and others.
  //   2) Ability to log seccomp filter failures. Without this, it is hard to
  //      know which syscall has to be added to the filter policy file.
  auto timer = timer_type != TIMER_NONE
                   ? base::MakeUnique<ScopedTimerReporter>(timer_type)
                   : nullptr;
  ScopedSwitchToSavedUid switch_scope;
  return cmd->Execute();
}

void JailHelper::SetDebugFlags(bool disable_seccomp_filters,
                               bool log_seccomp_filters) {
  disable_seccomp_filters_ = disable_seccomp_filters;
  log_seccomp_filters_ = log_seccomp_filters;
}

}  // namespace authpolicy
