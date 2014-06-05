// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_exit_handler.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>

#include "login_manager/child_job.h"
#include "login_manager/job_manager.h"
#include "login_manager/system_utils.h"

namespace login_manager {

namespace {
static int g_child_pipe_write_fd = -1;
static int g_child_pipe_read_fd = -1;

void SIGCHLDHandler(int signal, siginfo_t* info, void*) {
  RAW_CHECK(signal == SIGCHLD);
  RAW_LOG(INFO, "Handling SIGCHLD.");

  RAW_CHECK(g_child_pipe_write_fd != -1);
  RAW_CHECK(g_child_pipe_read_fd != -1);

  SystemUtils::RetryingWrite(g_child_pipe_write_fd,
                             reinterpret_cast<const char*>(info),
                             sizeof(siginfo_t));
  RAW_LOG(INFO, "Successfully wrote to child pipe.");
}

}  // anonymous namespace

ChildExitHandler::ChildExitHandler(SystemUtils* system)
    : fd_watcher_(new base::MessageLoopForIO::FileDescriptorWatcher),
      system_(system) {
  int pipefd[2];
  PLOG_IF(FATAL, pipe2(pipefd,
                       O_CLOEXEC | O_NONBLOCK) < 0) << "Failed to create pipe";
  g_child_pipe_read_fd = pipefd[0];
  g_child_pipe_write_fd = pipefd[1];
}

ChildExitHandler::~ChildExitHandler() {
  RevertHandlers();
  close(g_child_pipe_write_fd);
  g_child_pipe_write_fd = -1;
  close(g_child_pipe_read_fd);
  g_child_pipe_read_fd = -1;
}

void ChildExitHandler::Init(const std::vector<JobManagerInterface*>& managers) {
  managers_ = managers;

  SetUpHandler();
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          g_child_pipe_read_fd, true, base::MessageLoopForIO::WATCH_READ,
          fd_watcher_.get(), this)) {
    LOG(FATAL) << "Watching child pipe failed. Can't manage browser process.";
  }
}

void ChildExitHandler::OnFileCanReadWithoutBlocking(int fd) {
  siginfo_t info;
  while (base::ReadFromFD(fd, reinterpret_cast<char*>(&info), sizeof(info)))
    DCHECK(info.si_signo == SIGCHLD) << "Wrong signal!";

  // Reap all terminated children.
  while (true) {
    memset(&info, 0, sizeof(info));
    int result = waitid(P_ALL, 0, &info, WEXITED | WNOHANG);
    if (result != 0) {
      if (errno != ECHILD)
        PLOG(FATAL) << "waitid failed";
      break;
    }
    if (info.si_pid == 0)
      break;
    Dispatch(info);
  }
}

void ChildExitHandler::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

// static
void ChildExitHandler::RevertHandlers() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);
}

void ChildExitHandler::Dispatch(const siginfo_t& info) {
  // Find the manager whose child has exited.
  std::vector<JobManagerInterface*>::iterator job_manager = managers_.begin();
  while (job_manager != managers_.end()) {
    if ((*job_manager)->IsManagedJob(info.si_pid))
      break;
    ++job_manager;
  }
  if (job_manager == managers_.end()) {
    DLOG(INFO) << info.si_pid <<  " is not a managed job.";
    return;
  }

  LOG(INFO) << "Handling " << info.si_pid << " exit.";
  if (info.si_code == CLD_EXITED) {
    LOG_IF(ERROR, info.si_status != 0) << "  Exited with exit code "
                                       << info.si_status;
    CHECK(info.si_status != ChildJobInterface::kCantSetUid);
    CHECK(info.si_status != ChildJobInterface::kCantSetEnv);
    CHECK(info.si_status != ChildJobInterface::kCantExec);
  } else {
    LOG(ERROR) << "  Exited with signal " << info.si_status;
  }
  (*job_manager)->HandleExit(info);
}

void ChildExitHandler::SetUpHandler() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));

  // Manually set an action for SIGCHLD, so that we can convert
  // receiving a signal on child exit to file descriptor
  // activity. This way we can handle this kind of event in a
  // MessageLoop.  The flags are set such that the action receives a
  // siginfo struct with handy exit status info, but only when the
  // child actually exits -- as opposed to also when it gets STOP or CONT.
  memset(&action, 0, sizeof(action));
  action.sa_flags = (SA_SIGINFO | SA_NOCLDSTOP);
  action.sa_sigaction = SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);
}

}  // namespace login_manager
