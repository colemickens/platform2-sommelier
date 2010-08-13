// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
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

#include "login_manager/file_checker.h"
#include "login_manager/system_utils.h"

class CommandLine;

namespace login_manager {
namespace gobject {
struct SessionManager;
}  // namespace gobject

class ChildJobInterface;

// Provides a wrapper for exporting SessionManagerInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
class SessionManagerService : public chromeos::dbus::AbstractDbusService {
 public:
  SessionManagerService(std::vector<ChildJobInterface*> child_jobs);
  virtual ~SessionManagerService();

  // If you want to call any of these setters, you should do so before calling
  // any other methods on this class.
  class TestApi {
   public:
    // Allows a test program to set the pid of a child.
    void set_child_pid(int i_child, int pid) {
      session_manager_service_->child_pids_[i_child] = pid;
    }

    // Set the SystemUtils used by the manager. Useful for mocking.
    void set_systemutils(SystemUtils* utils) {
      session_manager_service_->system_.reset(utils);
    }

    // Sets whether the the manager exits when a child finishes.
    void set_exit_on_child_done(bool do_exit) {
      session_manager_service_->exit_on_child_done_ = do_exit;
    }

    // Executes the CleanupChildren() method on the manager.
    void CleanupChildren(int timeout) {
      session_manager_service_->CleanupChildren(timeout);
    }

    // Sets whether the screen is locked.
    // TODO(davemoore) Need to come up with a way to mock dbus so we can
    // better test this functionality.
    void set_screen_locked(bool screen_locked) {
      session_manager_service_->screen_locked_ = screen_locked;
    }

   private:
    friend class SessionManagerService;
    explicit TestApi(SessionManagerService* session_manager_service)
        : session_manager_service_(session_manager_service) {}
    SessionManagerService* session_manager_service_;
  };

  // TestApi exposes internal routines for testing purposes.
  TestApi test_api() { return TestApi(this); }
  ////////////////////////////////////////////////////////////////////////////
  // Implementing chromeos::dbus::AbstractDbusService
  virtual bool Initialize();
  virtual bool Reset();

  // Takes ownership of |file_checker|
  void set_file_checker(FileChecker* file_checker) {
    file_checker_.reset(file_checker);
  }

  // Can't be "unset".
  void set_uid(uid_t uid) {
    uid_ = uid;
    set_uid_ = true;
  }

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

  // Fork, then call child_job_->Run() in the child and set a
  // babysitter in the parent's glib default context that calls
  // HandleChildExit when the child is done.
  void RunChildren();

  // Run one of the children.
  int RunChild(ChildJobInterface* child_job);

  // Tell us that, if we want, we can cause a graceful exit from g_main_loop.
  void AllowGracefulExit();

  ////////////////////////////////////////////////////////////////////////////
  // SessionManagerService commands

  // Emits the "login-prompt-ready" upstart signal.
  gboolean EmitLoginPromptReady(gboolean* OUT_emitted, GError** error);

  // In addition to emitting "start-user-session" upstart signal and
  // "SessionStateChanged:started" D-Bus signal, this function will
  // also call child_job_->StartSession(email_address).
  gboolean StartSession(gchar* email_address,
                        gchar* unique_identifier,
                        gboolean* OUT_done,
                        GError** error);

  // In addition to emitting "stop-user-session", this function will
  // also call child_job_->StopSession().
  gboolean StopSession(gchar* unique_identifier,
                       gboolean* OUT_done,
                       GError** error);

  // Handles LockScreen request from PowerManager. It switches itself to
  // lock mode, and emit LockScreen signal to Chromium Browser.
  gboolean LockScreen(GError** error);

  // Handles UnlockScreen request from PowerManager. It switches itself to
  // unlock mode, and emit UnlockScreen signal to Chromium Browser.
  gboolean UnlockScreen(GError** error);

  // Restarts job with specified pid replacing its command line arguments
  // with provided.
  gboolean RestartJob(gint pid,
                      gchar* arguments,
                      gboolean* OUT_done,
                      GError** error);

  // Perform very, very basic validation of |email_address|.
  static bool ValidateEmail(const std::string& email_address);

  // Breaks |args| into separate arg lists, delimited by "--".
  // No initial "--" is needed, but is allowed.
  // ("a", "b", "c") => ("a", "b", "c")
  // ("a", "b", "c", "--", "d", "e", "f") =>
  //     ("a", "b", "c"), ("d", "e", "f").
  // Converts args from wide to plain strings.
  static std::vector<std::vector<std::string> > GetArgLists(
      std::vector<std::string> args);

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

  // Setup any necessary signal handlers.
  void SetupHandlers();

  // Terminate all children, with increasing prejudice.
  void CleanupChildren(int timeout);

  void SetGError(GError** error, ChromeOSLoginError, const char* message);

  // Sends a given signal to Chromium browser.
  void SendSignalToChromium(const char* signal_name);

  bool ShouldRunChildren();
  // Returns true if |child_job| believes it should be stopped.
  // If the child believes it should be stopped (as opposed to not run anymore)
  // we actually exit the Service as well.
  bool ShouldStopChild(ChildJobInterface* child_job);

  static const uint32 kMaxEmailSize;
  static const char kEmailSeparator;
  static const char kLegalCharacters[];
  static const char kIncognitoUser[];

  std::vector<ChildJobInterface*> child_jobs_;
  std::vector<int> child_pids_;
  bool exit_on_child_done_;

  gobject::SessionManager* session_manager_;
  GMainLoop* main_loop_;

  scoped_ptr<SystemUtils> system_;

  bool session_started_;

  // D-Bus GLib signal ids.
  guint signals_[kNumSignals];

  scoped_ptr<FileChecker> file_checker_;
  bool screen_locked_;
  uid_t uid_;
  bool set_uid_;

  bool shutting_down_;

  friend class TestAPI;
  DISALLOW_COPY_AND_ASSIGN(SessionManagerService);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
