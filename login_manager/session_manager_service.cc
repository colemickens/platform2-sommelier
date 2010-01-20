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
#include <base/string_util.h>
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

using std::string;

//static
const uint32 SessionManagerService::kMaxEmailSize = 200;
//static
const char SessionManagerService::kEmailSeparator = '@';
//static
const char SessionManagerService::kLegalCharacters[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    ".@1234567890";

SessionManagerService::SessionManagerService(ChildJob* child)
    : child_job_(child),
      exit_on_child_done_(false),
      child_pgid_(0),
      main_loop_(g_main_loop_new(NULL, FALSE)),
      system_(new SystemUtils),
      session_started_(false) {
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

  if (should_run_child()) {
    int pid = RunChild();
    if (pid == -1) {
      // We couldn't fork...maybe we should wait and try again later?
      PLOG(ERROR) << "Failed to fork!";
      return false;
    }
    child_pgid_ = -pid;
  } else {
    AllowGracefulExit();
  }

  // In the parent.
  g_main_loop_run(main_loop_);

  if (child_pgid_ != 0)  // otherwise, we never created a child.
    CleanupChildren(3);

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
  *OUT_emitted = system("/sbin/initctl emit login-prompt-ready &") == 0;
  if (*OUT_emitted) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_EMIT_FAILED,
              "Can't emit login-prompt-ready.");
  }
  return *OUT_emitted;
}

gboolean SessionManagerService::StartSession(gchar *email_address,
                                             gchar *unique_identifier,
                                             gboolean *OUT_done,
                                             GError **error) {
  if (session_started_) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_SESSION_EXISTS,
              "Can't start a session while a session is already active.");
    *OUT_done = FALSE;
    return FALSE;
  }
  // basic validity checking; avoid buffer overflows here, and
  // canonicalize the email address a little.
  char email[kMaxEmailSize + 1];
  snprintf(email, sizeof(email), "%s", email_address);
  email[kMaxEmailSize] = '\0';  // Just to be sure.
  string email_string(email);
  if (!ValidateEmail(email_string)) {
    *OUT_done = FALSE;
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_INVALID_EMAIL,
              "Provided email address is not valid.  ASCII only.");
    return FALSE;
  }
  string email_lower = StringToLowerASCII(email_string);
  DLOG(INFO) << "emitting start-user-session for " << email_lower;
  string command =
      StringPrintf("/sbin/initctl emit start-user-session CHROMEOS_USER=%s &",
                   email_lower.c_str());
  *OUT_done = system(command.c_str()) == 0;
  if (*OUT_done) {
    child_job_->Toggle();
    session_started_ = true;
  } else {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_EMIT_FAILED,
              "Can't emit start-session.");
  }
  return *OUT_done;
}

gboolean SessionManagerService::StopSession(gchar *unique_identifier,
                                            gboolean *OUT_done,
                                            GError **error) {
  DLOG(INFO) << "emitting stop-user-session";
  *OUT_done = system("/sbin/initctl emit stop-user-session &") == 0;
  if (*OUT_done) {
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    ServiceShutdown,
                    this,
                    NULL);
    child_job_->Toggle();
    session_started_ = false;
  } else {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_EMIT_FAILED,
              "Can't emit stop-session.");
  }
  return *OUT_done;
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
    // TODO(cmasone): deal with fork failing in RunChild()
    manager->set_child_pgid(-manager->RunChild());
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

// This can probably be more efficient, if it needs to be.
// static
bool SessionManagerService::ValidateEmail(const string& email_address) {
  if (email_address.find_first_not_of(kLegalCharacters) != string::npos)
    return false;

  size_t at = email_address.find(kEmailSeparator);
  // it has NO @.
  if (at == string::npos)
    return false;

  // it has more than one @.
  if (email_address.find(kEmailSeparator, at+1) != string::npos)
    return false;

  return true;
}

void SessionManagerService::SetupHandlers() {
  // I have to ignore SIGUSR1, because Xorg sends it to this process when it's
  // got no clients and is ready for new ones.  If we don't ignore it, we die.
  struct sigaction chld_action;
  memset(&chld_action, 0, sizeof(chld_action));
  chld_action.sa_handler = SIG_IGN;
  CHECK(sigaction(SIGUSR1, &chld_action, NULL) == 0);
}

void SessionManagerService::CleanupChildren(int max_tries) {
  int try_count = 0;
  while(!system_->child_is_gone(child_pgid_)) {
    system_->kill(child_pgid_, (try_count++ >= max_tries ? SIGKILL : SIGTERM));
    // TODO(cmasone): add conversion constants/methods in common/ somewhere.
    usleep(500 * 1000 /* milliseconds */);
  }
}

void SessionManagerService::SetGError(GError** error,
                                      ChromeOSLoginError code,
                                      const char* message) {
  g_set_error(error, CHROMEOS_LOGIN_ERROR, code, "Login error: %s", message);
}

}  // namespace login_manager
