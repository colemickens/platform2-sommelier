// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CHILD_EXIT_HANDLER_H_
#define LOGIN_MANAGER_CHILD_EXIT_HANDLER_H_

#include <signal.h>
#include <sys/types.h>

#include <string>

namespace login_manager {

// An interface for handling a child process exit.
class ChildExitHandler {
 public:
  virtual ~ChildExitHandler() = default;

  // Called when a child process has exited.
  // Returns true if it consumes the exit event (i.e., no more following
  // handlers will be notified by ChildExitDispatcher). Otherwise false.
  virtual bool HandleExit(const siginfo_t& status) = 0;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHILD_EXIT_HANDLER_H_
