// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_H_
#define LOGIN_MANAGER_SESSION_MANAGER_H_

#include <gtest/gtest.h>

#include <signal.h>
#include <unistd.h>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/scoped_ptr.h>
#include "login_manager/ipc_channel.h"

class CommandLine;

namespace login_manager {
class ChildJob;
class FileChecker;

class SessionManager {
 public:
  // Takes ownership of |checker|, |reader|, |child|.
  SessionManager(FileChecker* checker,
                 IpcReadChannel* reader,
                 ChildJob* child,
                 bool expect_ipc);
  virtual ~SessionManager();

  // To allow signal handlers to communicate back to this object.
  static volatile sig_atomic_t child_exited;

  // Runs the command specified on the command line as |desired_uid_| and
  // watches it, restarting it whenever it exits abnormally -- UNLESS
  // |magic_chrome_file| exists.
  //
  // So, this function will run until one of the following occurs:
  // 1) the specified command exits normally
  // 2) |magic_chrome_file| exists AND the specified command exits for any
  //     reason
  // 3) We can't fork/exec/setuid to |desired_uid_|
  void LoopChrome(const char magic_chrome_file[]);

  int num_loops() { return num_loops_; }  // exposed for testing.

 private:
  bool check_child_for_exit(pid_t pid, int* status) {
    return waitpid(pid, status, WNOHANG) == 0;
  }
  void block_on_child_exit(pid_t pid, int* status) {
    // wait for our direct child, grab status.
    while (waitpid(pid, status, 0) == -1 && errno == EINTR)
      ;
    // If I could wait for descendants here, I would.  Instead, I kill them.
    kill(-pid, SIGKILL);
  }

  void WatchIpcAndHandleMessages();
  bool HandleMessage(IpcMessage message);

  // These use initctl to send signals to upstart.
  void EmitLoginPromptReady();
  // In addition to sending start-user-session, this function will also toggle
  // presence of |kLoginManagerFlag| on the command line that gets executed.
  void EmitStartUserSession();

  void SetupHandlers();

  scoped_ptr<FileChecker> checker_;
  scoped_ptr<IpcReadChannel> reader_;
  scoped_ptr<ChildJob> child_job_;

  // TODO(cmasone): This flag allows us to work around the fact that
  // Chrome does not yet support sending a STOP_SESSION message.  In
  // fact, if we don't start it as a login manager, it won't IPC to us
  // at all.  That'd be bad.  Once we switch to DBus and fill out
  // Chrome's functionality in this space, this flag can go away.
  bool expect_ipc_;

  int num_loops_;  // for testing.

  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SESSION_MANAGER_H_
