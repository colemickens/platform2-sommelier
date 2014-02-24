// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CHILD_EXIT_HANDLER_H_
#define LOGIN_MANAGER_CHILD_EXIT_HANDLER_H_

#include <signal.h>

#include <vector>

#include <base/basictypes.h>
#include <base/compiler_specific.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>

namespace login_manager {
class JobManagerInterface;
class SystemUtils;

// Sets up signal handler for SIGCHLD, and converts signal receipt
// into a write on a pipe. The data written contains the child's exit status.
// Also watches the other end of this pipe and, when data appears, the info
// is read and the appropriate object that manages that child is informed.
class ChildExitHandler : public base::MessageLoopForIO::Watcher {
 public:
  explicit ChildExitHandler(SystemUtils* system);
  virtual ~ChildExitHandler();

  void Init(const std::vector<JobManagerInterface*>& managers);

  // Implementation of Watcher
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  // Revert signal handlers registered by this class.
  static void RevertHandlers();

 private:
  // Determines which JobManagers' child exited, and handles appropriately.
  void Dispatch(const siginfo_t& info);

  // Set up handler for SIGCHLD.
  void SetUpHandler();

  // Objects that are managing a child job.
  std::vector<JobManagerInterface*> managers_;

  // Controller used to manage watching of shutdown pipe.
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  SystemUtils* system_;
  DISALLOW_COPY_AND_ASSIGN(ChildExitHandler);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHILD_EXIT_HANDLER_H_
