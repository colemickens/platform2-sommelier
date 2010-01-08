// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager.h"

#include <grp.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>

#include "login_manager/child_job.h"
#include "login_manager/ipc_channel.h"
#include "login_manager/file_checker.h"

namespace login_manager {

SessionManager::SessionManager(FileChecker* checker,
                               IpcReadChannel* reader,
                               ChildJob* child,
                               bool expect_ipc)
    : checker_(checker),
      reader_(reader),
      child_job_(child),
      expect_ipc_(expect_ipc),
      num_loops_(0) {
  if (checker == NULL)
    checker_.reset(new FileChecker);
  CHECK(child);
  SetupHandlers();
}

SessionManager::~SessionManager() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
}

void SessionManager::LoopChrome(const char magic_chrome_file[]) {
  bool keep_going = true;
  while (keep_going && !checker_->exists(magic_chrome_file)) {

    LOG(INFO) << "Try " <<  num_loops_;
    int pid = fork();
    if (pid == -1) {
      // We couldn't fork...maybe we should wait and try again later?
      // Right now, we stop looping.
      keep_going = false;
      LOG(ERROR) << "  errno: " << strerror(errno);

    } else if (pid == 0) {
      // In the child.
      child_job_->Run();

    } else {
      // In the parent.

      ++num_loops_;
      // If we're supposed to be listening for IPC from the child, we
      // want to loop around, waiting for data to come in over the
      // pipe, until the child exits and/or closes the pipe.
      int status;
      if (check_child_for_exit(pid, &status)) {
        if (expect_ipc_) {
          LOG(INFO) << pid << " hasn't exited yet, waiting for IPC";
          WatchIpcAndHandleMessages();
        }

        // If either we're not doing IPC, or the child has closed the
        // pipe but is still alive, we wish to wait for him.  And, if
        // the child DID exit, then we'll just blow right through the
        // waitpid anyway.
        LOG(INFO) << "Stopped IPC, waiting for an exit now";
        block_on_child_exit(pid, &status);
      }

      // If the child was killed by an unhandled signal, or exited uncleanly,
      // we want to start it up again.
      keep_going = WIFSIGNALED(status) ||
                   (WIFEXITED(status) && WEXITSTATUS(status) != 0);

      DLOG(INFO) << "exited waitpid.\n"
                 << "  WIFSIGNALED is " << WIFSIGNALED(status) << "\n"
                 << "  WTERMSIG is " << WTERMSIG(status) << "\n"
                 << "  WIFEXITED is " << WIFEXITED(status) << "\n"
                 << "  WEXITSTATUS is " << WEXITSTATUS(status);
      if (WIFEXITED(status)) {
        CHECK(WEXITSTATUS(status) != SetUidExecJob::kCantSetuid);
        CHECK(WEXITSTATUS(status) != SetUidExecJob::kCantExec);
      }
      LOG(INFO) << pid << " has exited, keep_going is " << keep_going;
    }
  }
}

void SessionManager::WatchIpcAndHandleMessages() {
  if (reader_.get()) {
    bool stop_listening = false;
    reader_->init();
    LOG(INFO) << "Starting to listen for IPC";
    while(!stop_listening) {
      IpcMessage message = reader_->recv();
      stop_listening = !HandleMessage(message) && reader_->channel_eof();
    }
    reader_->shutdown();
  }
}

bool SessionManager::HandleMessage(IpcMessage message) {
  bool return_code = true;
  switch(message)
  {
    case EMIT_LOGIN:
      EmitLoginPromptReady();
      break;
    case START_SESSION:
      EmitStartUserSession();
      child_job_->Toggle();
      expect_ipc_ = !expect_ipc_;
      break;
    case STOP_SESSION:
      //EmitStopUserSession();  // We don't use this, yet.
      child_job_->Toggle();
      break;
    case FAILED:
      return_code = false;
      break;
    default:
      LOG(ERROR) << "Unknown message: " << message;
      return_code = false;
      break;
  }
  return return_code;
}

void SessionManager::EmitLoginPromptReady() {
  DLOG(INFO) << "emitting login-prompt-ready ";
  system("/sbin/initctl emit login-prompt-ready &");
}

void SessionManager::EmitStartUserSession() {
  DLOG(INFO) << "emitting start-user-session";
  system("/sbin/initctl emit start-user-session &");
}

void SessionManager::SetupHandlers() {
  // I have to ignore SIGUSR1, because Xorg sends it to this process when it's
  // got no clients and is ready for new ones.  If we don't ignore it, we die.
  struct sigaction chld_action;
  memset(&chld_action, 0, sizeof(chld_action));
  chld_action.sa_handler = SIG_IGN;
  CHECK(sigaction(SIGUSR1, &chld_action, NULL) == 0);
}

}  // namespace login_manager
