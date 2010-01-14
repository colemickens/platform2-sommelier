// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <glib.h>
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
#include <chromeos/dbus/dbus.h>

#include "login_manager/child_job.h"
#include "login_manager/interface.h"

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace login_manager {  // NOLINT
namespace gobject {  // NOLINT
#include "login_manager/bindings/server.h"
}  // namespace gobject
}  // namespace login_manager

namespace login_manager {

SessionManagerService::SessionManagerService(ChildJob* child)
    : child_job_(child),
      exit_on_child_done_(false),
      main_loop_(g_main_loop_new(NULL, FALSE)) {
  CHECK(child);
  SetupHandlers();
}

SessionManagerService::SessionManagerService(ChildJob* child,
                                             bool exit_on_child_done)
    : child_job_(child),
      exit_on_child_done_(exit_on_child_done),
      main_loop_(g_main_loop_new(NULL, FALSE)) {
  CHECK(child);
  SetupHandlers();
}

SessionManagerService::~SessionManagerService() {
  g_main_loop_unref(main_loop_);

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
}

bool SessionManagerService::Initialize() {
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(
      gobject::session_manager_get_type(),
      &gobject::dbus_glib_session_manager_object_info);
  return Reset();
}

bool SessionManagerService::Reset() {
  session_manager_.reset(NULL);  // Make sure the destructor is run first.
  session_manager_.reset(
      reinterpret_cast<gobject::SessionManager*>(
          g_object_new(gobject::session_manager_get_type(), NULL)));

  // Allow references to this instance.
  session_manager_->service = this;

  if (main_loop_) {
    ::g_main_loop_unref(main_loop_);
  }
  main_loop_ = g_main_loop_new(NULL, false);
  if (!main_loop_) {
    LOG(ERROR) << "Failed to create main loop";
    return false;
  }
  return true;
}

bool SessionManagerService::Run() {
  if (!main_loop_) {
    LOG(ERROR) << "You must have a main loop to call Run.";
    return false;
  }
  int pid = RunChild();
  if (pid == -1) {
    // We couldn't fork...maybe we should wait and try again later?
    PLOG(ERROR) << "Failed to fork!";

  } else {
    // In the parent.
    g_main_loop_run(main_loop_);
  }
  return true;
}

int SessionManagerService::RunChild() {
  int pid = fork();
  if (pid == 0) {
    // In the child.
    child_job_->Run();
    exit(1);  // Run() is not supposed to return.
  }
  g_child_watch_add_full(G_PRIORITY_HIGH_IDLE,
                         pid,
                         HandleChildExit,
                         this,
                         NULL);
  return pid;
}

void SessionManagerService::AllowGracefulExit() {
  if (exit_on_child_done_) {
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    ServiceShutdown,
                    this,
                    NULL);
  }
}

///////////////////////////////////////////////////////////////////////////////
// SessionManagerService commands

gboolean SessionManagerService::EmitLoginPromptReady(gboolean *OUT_emitted,
                                                     GError **error) {
  DLOG(INFO) << "emitting login-prompt-ready ";
  system("/sbin/initctl emit login-prompt-ready &");
  *OUT_emitted = TRUE;
  return TRUE;
}

gboolean SessionManagerService::StartSession(gchar *email_address,
                                             gchar *unique_identifier,
                                             gboolean *OUT_done,
                                             GError **error) {
  DLOG(INFO) << "emitting start-user-session";
  system("/sbin/initctl emit start-user-session &");
  child_job_->Toggle();
  *OUT_done = TRUE;
  return TRUE;
}

gboolean SessionManagerService::StopSession(gchar *unique_identifier,
                                            gboolean *OUT_done,
                                            GError **error) {
  DLOG(INFO) << "emitting stop-user-session";
  system("/sbin/initctl emit stop-user-session &");
  g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                  ServiceShutdown,
                  this,
                  NULL);
  child_job_->Toggle();
  *OUT_done = TRUE;
  return TRUE;
}


///////////////////////////////////////////////////////////////////////////////
// glib event handlers

void SessionManagerService::HandleChildExit(GPid pid,
                                            gint status,
                                            gpointer data) {
  // If I could wait for descendants here, I would.  Instead, I kill them.
  kill(-pid, SIGKILL);

  DLOG(INFO) << "exited waitpid.\n"
             << "  WIFSIGNALED is " << WIFSIGNALED(status) << "\n"
             << "  WTERMSIG is " << WTERMSIG(status) << "\n"
             << "  WIFEXITED is " << WIFEXITED(status) << "\n"
             << "  WEXITSTATUS is " << WEXITSTATUS(status);
  if (WIFEXITED(status)) {
    CHECK(WEXITSTATUS(status) != SetUidExecJob::kCantSetuid);
    CHECK(WEXITSTATUS(status) != SetUidExecJob::kCantExec);
  }

  // If the child _ever_ exits, we want to start it up again.
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  if (manager->should_run_child()) {
    manager->RunChild();
  } else {
    LOG(INFO) << "Should NOT run";
    manager->AllowGracefulExit();
  }
}

gboolean SessionManagerService::ServiceShutdown(gpointer data) {
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  manager->Shutdown();
  return FALSE;  // So that the event source that called this gets removed.
}



///////////////////////////////////////////////////////////////////////////////
// Utility Methods

void SessionManagerService::SetupHandlers() {
  // I have to ignore SIGUSR1, because Xorg sends it to this process when it's
  // got no clients and is ready for new ones.  If we don't ignore it, we die.
  struct sigaction chld_action;
  memset(&chld_action, 0, sizeof(chld_action));
  chld_action.sa_handler = SIG_IGN;
  CHECK(sigaction(SIGUSR1, &chld_action, NULL) == 0);
}

}  // namespace login_manager
