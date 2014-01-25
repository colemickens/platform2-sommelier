// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/termination_handler.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/logging.h>
#include <glib.h>

#include "login_manager/process_manager_service_interface.h"
#include "login_manager/system_utils.h"
#include "login_manager/watcher.h"

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
    : manager_(manager) {
  int pipefd[2];
  PLOG_IF(DFATAL, pipe2(pipefd, O_CLOEXEC) < 0) << "Failed to create pipe";
  g_shutdown_pipe_read_fd = pipefd[0];
  g_shutdown_pipe_write_fd = pipefd[1];
}

TerminationHandler::~TerminationHandler() {}

void TerminationHandler::Init() {
  SetUpHandlers();
  g_io_add_watch_full(g_io_channel_unix_new(g_shutdown_pipe_read_fd),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP),
                      HandleKill,
                      this,
                      NULL);
}

void TerminationHandler::OnFileCanReadWithoutBlocking(int fd) {
  // We only get called if there's data on the pipe.  If there's data, we're
  // supposed to exit.  So, don't even bother to read it.
  LOG(INFO) << "HUP, INT, or TERM received; exiting.";
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

// static
gboolean TerminationHandler::HandleKill(GIOChannel* source,
                                        GIOCondition condition,
                                        gpointer data) {
  CHECK(G_IO_IN & condition);
  Watcher* handler = static_cast<Watcher*>(data);
  handler->OnFileCanReadWithoutBlocking(g_io_channel_unix_get_fd(source));
  return FALSE;  // So that the event source that called this gets removed.
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
