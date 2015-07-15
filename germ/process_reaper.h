// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_PROCESS_REAPER_H_
#define GERM_PROCESS_REAPER_H_

#include <sys/wait.h>

#include <base/macros.h>
#include <chromeos/asynchronous_signal_handler.h>
#include <chromeos/daemons/daemon.h>

namespace germ {

class ProcessReaper {
 public:
  ProcessReaper();
  virtual ~ProcessReaper();

  void RegisterWithAsyncHandler(
      chromeos::AsynchronousSignalHandler* async_signal_handler);
  void RegisterWithDaemon(chromeos::Daemon* daemon);

 private:
  // SIGCHLD handler for the AsynchronousSignalHandler. Always returns false
  // (meaning that the signal handler should not be unregistered).
  bool HandleSIGCHLD(const struct signalfd_siginfo& sigfd_info);

  // Called when a child has been successfully reaped.
  virtual void HandleReapedChild(const siginfo_t& info);

  DISALLOW_COPY_AND_ASSIGN(ProcessReaper);
};

}  // namespace germ

#endif  // GERM_PROCESS_REAPER_H_
