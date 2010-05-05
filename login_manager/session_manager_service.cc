// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <errno.h>
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

// Jacked from chrome base/eintr_wrapper.h
#define HANDLE_EINTR(x) ({ \
  typeof(x) __eintr_result__; \
  do { \
    __eintr_result__ = x; \
  } while (__eintr_result__ == -1 && errno == EINTR); \
  __eintr_result__;\
})

int g_shutdown_pipe_write_fd = -1;
int g_shutdown_pipe_read_fd = -1;

// static
// Common code between SIG{HUP, INT, TERM}Handler.
void SessionManagerService::GracefulShutdownHandler(int signal) {
  // Reinstall the default handler.  We had one shot at graceful shutdown.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  RAW_CHECK(sigaction(signal, &action, NULL) == 0);

  RAW_CHECK(g_shutdown_pipe_write_fd != -1);
  RAW_CHECK(g_shutdown_pipe_read_fd != -1);
  size_t bytes_written = 0;
  do {
    int rv = HANDLE_EINTR(
        write(g_shutdown_pipe_write_fd,
              reinterpret_cast<const char*>(&signal) + bytes_written,
              sizeof(signal) - bytes_written));
    RAW_CHECK(rv >= 0);
    bytes_written += rv;
  } while (bytes_written < sizeof(signal));

  RAW_LOG(INFO,
          "Successfully wrote to shutdown pipe, resetting signal handler.");
}

// static
void SessionManagerService::SIGHUPHandler(int signal) {
  RAW_CHECK(signal == SIGHUP);
  RAW_LOG(INFO, "Handling SIGHUP.");
  GracefulShutdownHandler(signal);
}
// static
void SessionManagerService::SIGINTHandler(int signal) {
  RAW_CHECK(signal == SIGINT);
  RAW_LOG(INFO, "Handling SIGINT.");
  GracefulShutdownHandler(signal);
}

// static
void SessionManagerService::SIGTERMHandler(int signal) {
  RAW_CHECK(signal == SIGTERM);
  RAW_LOG(INFO, "Handling SIGTERM.");
  GracefulShutdownHandler(signal);
}

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
      child_pid_(0),
      session_manager_(NULL),
      main_loop_(g_main_loop_new(NULL, FALSE)),
      system_(new SystemUtils),
      session_started_(false) {
  CHECK(child);
  SetupHandlers();
}

SessionManagerService::~SessionManagerService() {
  if (main_loop_)
    g_main_loop_unref(main_loop_);
  if (session_manager_)
    g_object_unref(session_manager_);

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
  CHECK(sigaction(SIGALRM, &action, NULL) == 0);
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);
}

bool SessionManagerService::Initialize() {
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(
      gobject::session_manager_get_type(),
      &gobject::dbus_glib_session_manager_object_info);

  // Creates D-Bus GLib signal ids.
  signals_[kSignalSessionStateChanged] =
      g_signal_new("session_state_changed",
                   gobject::session_manager_get_type(),
                   G_SIGNAL_RUN_LAST,
                   0,
                   NULL, NULL,
                   g_cclosure_marshal_VOID__STRING,
                   G_TYPE_NONE, 1, G_TYPE_STRING);

  return Reset();
}

bool SessionManagerService::Reset() {
  if (session_manager_)
    g_object_unref(session_manager_);
  session_manager_ =
      reinterpret_cast<gobject::SessionManager*>(
          g_object_new(gobject::session_manager_get_type(), NULL));

  // Allow references to this instance.
  session_manager_->service = this;

  if (main_loop_)
    g_main_loop_unref(main_loop_);
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

  int pipefd[2];
  int ret = pipe(pipefd);
  if (ret < 0) {
    PLOG(DFATAL) << "Failed to create pipe";
  } else {
    g_shutdown_pipe_read_fd = pipefd[0];
    g_shutdown_pipe_write_fd = pipefd[1];
    g_io_add_watch_full(g_io_channel_unix_new(g_shutdown_pipe_read_fd),
                        G_PRIORITY_HIGH_IDLE,
                        GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP),
                        HandleKill,
                        this,
                        NULL);
  }

  if (should_run_child()) {
    int pid = RunChild();
    if (pid == -1) {
      // We couldn't fork...maybe we should wait and try again later?
      PLOG(ERROR) << "Failed to fork!";
      return false;
    }
    child_pid_ = pid;
  } else {
    AllowGracefulExit();
  }

  g_main_loop_run(main_loop_);

  if (child_pid_ != 0)  // otherwise, we never created a child.
    CleanupChildren(3);

  return true;
}

bool SessionManagerService::Shutdown() {
  if (session_started_) {
    DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:stopped";
    g_signal_emit(session_manager_,
                  signals_[kSignalSessionStateChanged],
                  0, "stopped");
  }

  return chromeos::dbus::AbstractDbusService::Shutdown();
}

int SessionManagerService::RunChild() {
  child_job_->RecordTime();
  int pid = fork();
  if (pid == 0) {
    // Log the time we execute chrome.
    system("set -o noclobber ; cat /proc/uptime > /tmp/uptime-chrome-exec");
    system("set -o noclobber ; cat /sys/block/sda/stat > /tmp/disk-chrome-exec");
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
  string command;
  if (child_job_->desired_uid_is_set()) {
    command = StringPrintf("/sbin/initctl emit start-user-session "
                           "CHROMEOS_USER=%s USER_ID=%d &",
                           email_lower.c_str(), child_job_->desired_uid());
  } else {
    command = StringPrintf("/sbin/initctl emit start-user-session "
                           "CHROMEOS_USER=%s &",
                           email_lower.c_str());
  }
  // TODO(yusukes,cmasone): set DATA_DIR variable as well?

  *OUT_done = system(command.c_str()) == 0;
  if (*OUT_done) {
    child_job_->SetState(email_lower);
    session_started_ = true;

    DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:started";
    g_signal_emit(session_manager_,
                  signals_[kSignalSessionStateChanged],
                  0, "started");
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
  g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                  ServiceShutdown,
                  this,
                  NULL);
  // TODO(cmasone): re-enable these when we try to enable logout without exiting
  //                the session manager
  // child_job_->SetSwitch(true);
  // session_started_ = false;
  return *OUT_done = TRUE;
}


///////////////////////////////////////////////////////////////////////////////
// glib event handlers

void SessionManagerService::HandleChildExit(GPid pid,
                                            gint status,
                                            gpointer data) {
  // If I could wait for descendants here, I would.  Instead, I kill them.
  kill(-pid, SIGKILL);

  DLOG(INFO) << "Handling child process exit.";
  if (WIFSIGNALED(status)) {
    DLOG(INFO) << "  Exited with signal " << WTERMSIG(status);
  } else if (WIFEXITED(status)) {
    DLOG(INFO) << "  Exited with exit code " << WEXITSTATUS(status);
    CHECK(WEXITSTATUS(status) != SetUidExecJob::kCantSetuid);
    CHECK(WEXITSTATUS(status) != SetUidExecJob::kCantExec);
  } else {
    DLOG(INFO) << "  Exited...somehow, without an exit code or a signal??";
  }

  bool exited_clean = WIFEXITED(status) && WEXITSTATUS(status) == 0;

  // If the child _ever_ exits uncleanly, we want to start it up again.
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  if (exited_clean || manager->should_stop_child()) {
    ServiceShutdown(data);
  } else if (manager->should_run_child()) {
    // TODO(cmasone): deal with fork failing in RunChild()
    LOG(INFO) << "Running the child again...";
    manager->set_child_pid(manager->RunChild());
  } else {
    LOG(INFO) << "Should NOT run";
    manager->AllowGracefulExit();
  }
}

gboolean SessionManagerService::HandleKill(GIOChannel* source,
                                           GIOCondition condition,
                                           gpointer data) {
  // We only get called if there's data on the pipe.  If there's data, we're
  // supposed to exit,.  So, don't even bother to read it.
  return ServiceShutdown(data);
}

gboolean SessionManagerService::ServiceShutdown(gpointer data) {
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  manager->Shutdown();
  LOG(INFO) << "SessionManagerService exiting";
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
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);

  action.sa_handler = SessionManagerService::do_nothing;
  CHECK(sigaction(SIGALRM, &action, NULL) == 0);

  // We need to handle SIGTERM, because that is how many POSIX-based distros ask
  // processes to quit gracefully at shutdown time.
  action.sa_handler = SIGTERMHandler;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  // Also handle SIGINT - when the user terminates the browser via Ctrl+C.
  // If the browser process is being debugged, GDB will catch the SIGINT first.
  action.sa_handler = SIGINTHandler;
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  // And SIGHUP, for when the terminal disappears. On shutdown, many Linux
  // distros send SIGHUP, SIGTERM, and then SIGKILL.
  action.sa_handler = SIGHUPHandler;
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);
}

void SessionManagerService::CleanupChildren(int timeout) {
  system_->kill(child_pid_, (session_started_ ? SIGTERM: SIGKILL));
  if (!system_->child_is_gone(child_pid_, timeout))
    system_->kill(child_pid_, SIGKILL);
}

void SessionManagerService::SetGError(GError** error,
                                      ChromeOSLoginError code,
                                      const char* message) {
  g_set_error(error, CHROMEOS_LOGIN_ERROR, code, "Login error: %s", message);
}

}  // namespace login_manager
