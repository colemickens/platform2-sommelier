// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/termination_handler.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/logging.h>
#include <base/message_loop/message_loop.h>

#include "login_manager/process_manager_service_interface.h"
#include "login_manager/system_utils.h"

namespace login_manager {

namespace {
static int g_shutdown_pipe_write_fd = -1;
static int g_shutdown_pipe_read_fd = -1;

// Common code between SIG{HUP,INT,TERM}Handler.
void GracefulShutdownHandler(int signal) {
  RAW_CHECK(g_shutdown_pipe_write_fd != -1);
  RAW_CHECK(g_shutdown_pipe_read_fd != -1);

  SystemUtils::RetryingWrite(g_shutdown_pipe_write_fd,
                             reinterpret_cast<const char*>(&signal),
                             sizeof(signal));
  RAW_LOG(INFO,
          "Successfully wrote to shutdown pipe, signal handler will be reset.");
}

void SIGHUPHandler(int signal) {
  RAW_CHECK(signal == SIGHUP);
  RAW_LOG(INFO, "Handling SIGHUP.");
  GracefulShutdownHandler(signal);
}

void SIGINTHandler(int signal) {
  RAW_CHECK(signal == SIGINT);
  RAW_LOG(INFO, "Handling SIGINT.");
  GracefulShutdownHandler(signal);
}

void SIGTERMHandler(int signal) {
  RAW_CHECK(signal == SIGTERM);
  RAW_LOG(INFO, "Handling SIGTERM.");
  GracefulShutdownHandler(signal);
}

}  // anonymous namespace

TerminationHandler::TerminationHandler(ProcessManagerServiceInterface* manager)
    : manager_(manager),
      fd_watcher_(new base::MessageLoopForIO::FileDescriptorWatcher) {
  int pipefd[2];
  PLOG_IF(DFATAL, pipe2(pipefd, O_CLOEXEC) < 0) << "Failed to create pipe";
  g_shutdown_pipe_read_fd = pipefd[0];
  g_shutdown_pipe_write_fd = pipefd[1];
}

TerminationHandler::~TerminationHandler() {}

void TerminationHandler::Init() {
  SetUpHandlers();
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          g_shutdown_pipe_read_fd, true, base::MessageLoopForIO::WATCH_READ,
          fd_watcher_.get(), this)) {
    LOG(ERROR) << "Watching shutdown pipe failed. Graceful exit impossible.";
  }
}

void TerminationHandler::OnFileCanReadWithoutBlocking(int fd) {
  // We only get called if there's data on the pipe.  If there's data, we're
  // supposed to exit.  So, don't even bother to read it.
  LOG(INFO) << "HUP, INT, or TERM received; exiting.";
  fd_watcher_->StopWatchingFileDescriptor();  // Ensure we're not called again.
  manager_->ScheduleShutdown();
}

void TerminationHandler::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

// static
void TerminationHandler::RevertHandlers() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);
}

void TerminationHandler::SetUpHandlers() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));

  // For all termination handlers, we want the default re-installed after
  // we get a shot at handling the signal.
  action.sa_flags = SA_RESETHAND;

  // We need to handle SIGTERM, because that is how many POSIX-based distros ask
  // processes to quit gracefully at shutdown time.
  action.sa_handler = SIGTERMHandler;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  // Also handle SIGINT, if session_manager is being run in the foreground.
  action.sa_handler = SIGINTHandler;
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  // And SIGHUP, for when the terminal disappears. On shutdown, many Linux
  // distros send SIGHUP, SIGTERM, and then SIGKILL.
  action.sa_handler = SIGHUPHandler;
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);
}

}  // namespace login_manager
