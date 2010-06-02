// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
#define LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_

#include <gtest/gtest.h>

#include <errno.h>
#include <glib.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/scoped_ptr.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

#include "login_manager/child_job.h"
#include "login_manager/system_utils.h"

class CommandLine;

namespace login_manager {
namespace gobject {
struct SessionManager;
}  // namespace gobject

class ChildJob;

// Provides a wrapper for exporting SessionManagerInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
class SessionManagerService : public chromeos::dbus::AbstractDbusService {
 public:
  // Takes ownership of |child|.
  explicit SessionManagerService(ChildJob* child);
  virtual ~SessionManagerService();

  ////////////////////////////////////////////////////////////////////////////
  // Implementing chromeos::dbus::AbstractDbusService
  virtual bool Initialize();
  virtual bool Reset();

  // Runs the command specified on the command line as |desired_uid_| and
  // watches it, restarting it whenever it exits abnormally -- UNLESS
  // |magic_chrome_file| exists.
  //
  // So, this function will run until one of the following occurs:
  // 1) the specified command exits normally
  // 2) |magic_chrome_file| exists AND the specified command exits for any
  //     reason
  // 3) We can't fork/exec/setuid to |desired_uid_|
  virtual bool Run();

  virtual const char* service_name() const {
    return kSessionManagerServiceName;
  }
  virtual const char* service_path() const {
    return kSessionManagerServicePath;
  }
  virtual const char* service_interface() const {
    return kSessionManagerInterface;
  }
  virtual GObject* service_object() const {
    return G_OBJECT(session_manager_);
  }

  // Emits "SessionStateChanged:stopped" D-Bus signal if applicable
  // before invoking the inherited method.
  virtual bool Shutdown();

  // If you want to call any of these setters, you should do so before calling
  // any other methods on this class.
  void set_child_pid(pid_t pid) { child_pid_ = pid; }
  void set_systemutils(SystemUtils* utils) { system_.reset(utils); }
  void set_exit_on_child_done(bool do_exit) { exit_on_child_done_ = do_exit; }

  // Returns true if |child_job_| believes it should be run.
  bool should_run_child() { return child_job_->ShouldRun(); }
  // Returns true if |child_job_| believes it should be stopped.
  // If the child believes it should be stopped (as opposed to not run anymore)
  // we actually exit the Service as well.
  bool should_stop_child() { return child_job_->ShouldStop(); }

  // Fork, then call child_job_->Run() in the child and set a
  // babysitter in the parent's glib default context that calls
  // HandleChildExit when the child is done.
  int RunChild();

  // Tell us that, if we want, we can cause a graceful exit from g_main_loop.
  void AllowGracefulExit();

  ////////////////////////////////////////////////////////////////////////////
  // SessionManagerService commands

  // Emits the "login-prompt-ready" upstart signal.
  gboolean EmitLoginPromptReady(gboolean* OUT_emitted, GError** error);

  // In addition to emitting "start-user-session" upstart signal and
  // "SessionStateChanged:started" D-Bus signal, this function will
  // also call child_job_->SetState(email_address).
  gboolean StartSession(gchar* email_address,
                        gchar* unique_identifier,
                        gboolean* OUT_done,
                        GError** error);

  // In addition to emitting "stop-user-session", this function will
  // also call child_job_->SetSwitch(true).
  gboolean StopSession(gchar* unique_identifier,
                       gboolean* OUT_done,
                       GError** error);

  // Handles LockScreen request from PowerManager. It switches itself to
  // lock mode, and emit LockScreen signal to Chromium Browser.
  gboolean LockScreen(GError** error);

  // Handles UnlockScreen request from PowerManager. It switches itself to
  // unlock mode, and emit UnlockScreen signal to Chromium Browser.
  gboolean UnlockScreen(GError** error);

 protected:
  virtual GMainLoop* main_loop() { return main_loop_; }

 private:
  // D-Bus signals.
  enum Signals {
    kSignalSessionStateChanged,
    kNumSignals
  };

  static void do_nothing(int sig) {}

  // Common code between SIG{HUP, INT, TERM}Handler.
  static void GracefulShutdownHandler(int signal);
  static void SIGHUPHandler(int signal);
  static void SIGINTHandler(int signal);
  static void SIGTERMHandler(int signal);

  // |data| is a SessionManagerService*
  static void HandleChildExit(GPid pid,
                              gint status,
                              gpointer data);

  // |data| is a SessionManagerService*.  This is a wrapper around
  // ServiceShutdown() so that we can register it as the callback for
  // when |source| has data to read.
  static gboolean HandleKill(GIOChannel* source,
                             GIOCondition condition,
                             gpointer data);

  // So that we can enqueue an event that will exit the main loop.
  // |data| is a SessionManagerService*
  static gboolean ServiceShutdown(gpointer data);

  // Perform very, very basic validation of |email_address|.
  static bool ValidateEmail(const std::string& email_address);

  // Setup any necessary signal handlers.
  void SetupHandlers();

  // Terminate all children, with increasing prejudice.
  void CleanupChildren(int timeout);

  void SetGError(GError** error, ChromeOSLoginError, const char* message);

  // Sends a given signal to Chromium browser.
  void SendSignalToChromium(const char* signal_name);

  static const uint32 kMaxEmailSize;
  static const char kEmailSeparator;
  static const char kLegalCharacters[];
  static const char kIncognitoUser[];

  scoped_ptr<ChildJob> child_job_;
  bool exit_on_child_done_;
  pid_t child_pid_;

  gobject::SessionManager* session_manager_;
  GMainLoop* main_loop_;

  scoped_ptr<SystemUtils> system_;

  bool session_started_;

  // D-Bus GLib signal ids.
  guint signals_[kNumSignals];

  FRIEND_TEST(SessionManagerTest, SessionNotStartedCleanupTest);
  FRIEND_TEST(SessionManagerTest, SessionNotStartedSlowKillCleanupTest);
  FRIEND_TEST(SessionManagerTest, SessionStartedCleanupTest);
  FRIEND_TEST(SessionManagerTest, SessionStartedSlowKillCleanupTest);
  FRIEND_TEST(SessionManagerTest, EmailAddressTest);
  FRIEND_TEST(SessionManagerTest, EmailAddressNonAsciiTest);
  FRIEND_TEST(SessionManagerTest, EmailAddressNoAtTest);
  FRIEND_TEST(SessionManagerTest, EmailAddressTooMuchAtTest);
  DISALLOW_COPY_AND_ASSIGN(SessionManagerService);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
