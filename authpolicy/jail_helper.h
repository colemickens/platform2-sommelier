// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_JAIL_HELPER_H_
#define AUTHPOLICY_JAIL_HELPER_H_

#include <base/macros.h>

#include "authpolicy/authpolicy_metrics.h"
#include "authpolicy/path_service.h"

namespace authpolicy {

namespace protos {
class DebugFlags;
}

class ProcessExecutor;

// Helper class for setting up a minijail and running a process.
class JailHelper {
 public:
  JailHelper(const PathService* path_service, const protos::DebugFlags* flags);

  // Sets up minijail and executes |cmd|. |seccomp_path_key| specifies the path
  // of the seccomp filter to use. |timer_type| is the UMA timer metric to
  // report. Passing |TIMER_NONE| won't report anything. Returns true if the
  // process ran successfully.
  bool SetupJailAndRun(ProcessExecutor* cmd,
                       Path seccomp_path_key,
                       TimerType timer_type) const;

 private:
  // File paths, not owned.
  const PathService* paths_ = nullptr;

  // Debug flags, not owned.
  const protos::DebugFlags* flags_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(JailHelper);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_JAIL_HELPER_H_
