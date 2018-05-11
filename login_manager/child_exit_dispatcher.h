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
class ChildExitHandler;

// Listen for SIGCHLD and informs the appropriate object that manages that
// child.
class ChildExitDispatcher {
 public:
  ChildExitDispatcher(brillo::AsynchronousSignalHandler* signal_handler,
                      const std::vector<ChildExitHandler*>& managers);
  ~ChildExitDispatcher();

 private:
  // Called by the |AsynchronousSignalHandler| when a new SIGCHLD is received.
  bool OnSigChld(const struct signalfd_siginfo& info);

  // Notifies ChildExitHandlers one at a time about the child exiting until
  // one reports that it's handled the exit.
  void Dispatch(const siginfo_t& info);

  // Handler that notifies of signals. Owned by the caller.
  brillo::AsynchronousSignalHandler* const signal_handler_;

  // Handlers that will be notified about child exit events.
  const std::vector<ChildExitHandler*> handlers_;

  DISALLOW_COPY_AND_ASSIGN(ChildExitDispatcher);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHILD_EXIT_DISPATCHER_H_
