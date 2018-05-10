// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CHILD_EXIT_DISPATCHER_H_
#define LOGIN_MANAGER_CHILD_EXIT_DISPATCHER_H_

#include <signal.h>

#include <vector>

#include <base/compiler_specific.h>
#include <base/macros.h>

struct signalfd_siginfo;

namespace brillo {
class AsynchronousSignalHandler;
}

namespace login_manager {
class JobManagerInterface;

// Listen for SIGCHLD and informs the appropriate object that manages that
// child.
class ChildExitDispatcher {
 public:
  ChildExitDispatcher(brillo::AsynchronousSignalHandler* signal_handler,
                      const std::vector<JobManagerInterface*>& managers);
  ~ChildExitDispatcher();

 private:
  // Called by the |AsynchronousSignalHandler| when a new SIGCHLD is received.
  bool OnSigChld(const struct signalfd_siginfo& info);

  // Determines which JobManagers' child exited, and handles appropriately.
  void Dispatch(const siginfo_t& info);

  // Handler that notifies of signals. Owned by the caller.
  brillo::AsynchronousSignalHandler* const signal_handler_;

  // Objects that are managing a child job.
  const std::vector<JobManagerInterface*> managers_;

  DISALLOW_COPY_AND_ASSIGN(ChildExitDispatcher);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHILD_EXIT_DISPATCHER_H_
