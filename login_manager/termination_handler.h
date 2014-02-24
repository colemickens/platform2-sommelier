// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_TERMINATION_HANDLER_H_
#define LOGIN_MANAGER_TERMINATION_HANDLER_H_

#include <base/basictypes.h>
#include <base/compiler_specific.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>

namespace login_manager {
class ProcessManagerServiceInterface;

// Sets up signal handlers for termination signals, and converts signal receipt
// into a write on a pipe. Watches that pipe for data and, when some appears,
// triggers process shutdown.
class TerminationHandler : public base::MessageLoopForIO::Watcher {
 public:
  explicit TerminationHandler(ProcessManagerServiceInterface* manager);
  virtual ~TerminationHandler();

  void Init();

  // Implementation of Watcher
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  // Revert signal handlers registered by this class.
  static void RevertHandlers();
 private:
  // Interface that allows process shutdown to be triggered.
  ProcessManagerServiceInterface* manager_;  // Owned by the caller.

  // Controller used to manage watching of shutdown pipe.
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  // Set up signal handlers for TERM, INT, and HUP.
  void SetUpHandlers();
  DISALLOW_COPY_AND_ASSIGN(TerminationHandler);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_TERMINATION_HANDLER_H_
