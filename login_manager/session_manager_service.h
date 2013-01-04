// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
#define LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_

#include <dbus/dbus.h>
#include <errno.h>
#include <glib.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/synchronization/waitable_event.h>
#include <base/time.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

#include "login_manager/child_job.h"
#include "login_manager/device_local_account_policy_service.h"
#include "login_manager/device_policy_service.h"
#include "login_manager/file_checker.h"
#include "login_manager/key_generator.h"
#include "login_manager/liveness_checker.h"
#include "login_manager/login_metrics.h"
#include "login_manager/owner_key_loss_mitigator.h"
#include "login_manager/policy_key.h"
#include "login_manager/process_manager_service_interface.h"
#include "login_manager/session_manager_impl.h"
#include "login_manager/session_manager_interface.h"
#include "login_manager/upstart_signal_emitter.h"
#include "login_manager/user_policy_service_factory.h"

class MessageLoop;

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace login_manager {
namespace gobject {
struct SessionManager;
}  // namespace gobject

class ChildJobInterface;
class NssUtil;
class PolicyService;
class SystemUtils;

// Provides a wrapper for exporting SessionManagerInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
//
// All signatures used in the methods of the ownership API are
// SHA1 with RSA encryption.
class SessionManagerService
    : public base::RefCountedThreadSafe<SessionManagerService>,
      public chromeos::dbus::AbstractDbusService,
      public login_manager::ProcessManagerServiceInterface {
 public:
  enum ExitCode {
    SUCCESS = 0,
    CRASH_WHILE_SCREEN_LOCKED = 1,
    CHILD_EXITING_TOO_FAST = 2
  };

  SessionManagerService(scoped_ptr<ChildJobInterface> child_job,
                        int kill_timeout,
                        bool enable_browser_abort_on_hang,
                        base::TimeDelta hang_detection_interval,
                        SystemUtils* system);
  virtual ~SessionManagerService();

  // If you want to call any of these setters, you should do so before calling
  // any other methods on this class.
  class TestApi {
   public:
    // Allows a test program to set the pid of the watched browser process.
    void set_browser_pid(pid_t pid) {
      session_manager_service_->browser_.pid = pid;
    }

    void set_systemutils(SystemUtils* utils) {
      session_manager_service_->system_ = utils;
    }
    void set_keygen(KeyGenerator* gen) {
      session_manager_service_->key_gen_.reset(gen);
    }
    void set_login_metrics(LoginMetrics* metrics) {
      session_manager_service_->login_metrics_.reset(metrics);
    }
    void set_liveness_checker(LivenessChecker* checker) {
      session_manager_service_->liveness_checker_.reset(checker);
    }
    void set_session_manager(SessionManagerInterface* impl) {
      session_manager_service_->impl_.reset(impl);
    }
    // Sets whether the the manager exits when a child finishes.
    void set_exit_on_child_done(bool do_exit) {
      session_manager_service_->exit_on_child_done_ = do_exit;
    }

    // Executes the CleanupChildren() method on the manager.
    void CleanupChildren(int timeout) {
      session_manager_service_->CleanupChildren(timeout);
    }

    // Cause handling of faked-out exit of a child process.
    void ScheduleChildExit(pid_t pid, int status);

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
  virtual bool Initialize() OVERRIDE;
  virtual bool Register(const chromeos::dbus::BusConnection &conn) OVERRIDE;
  virtual bool Reset() OVERRIDE;

  // Takes ownership of |file_checker|.
  void set_file_checker(FileChecker* file_checker) {
    file_checker_.reset(file_checker);
  }

  // Takes ownership of |mitigator|.
  void set_mitigator(OwnerKeyLossMitigator* mitigator) {
    mitigator_.reset(mitigator);
  }

  // Can't be "unset".
  void set_uid(uid_t uid) {
    uid_ = uid;
    set_uid_ = true;
  }

  ExitCode exit_code() { return exit_code_; }

  // Runs the command specified on the command line as |desired_uid_| and
  // watches it, restarting it whenever it exits abnormally -- UNLESS
  // |magic_chrome_file| exists.
  //
  // So, this function will run until one of the following occurs:
  // 1) the specified command exits normally
  // 2) |magic_chrome_file| exists AND the specified command exits for any
  //     reason
  // 3) We can't fork/exec/setuid to |desired_uid_|
  virtual bool Run() OVERRIDE;

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
  virtual bool Shutdown() OVERRIDE;

  // Implementing ProcessManagerServiceInterface
  virtual void ScheduleShutdown() OVERRIDE;
  virtual void RunBrowser() OVERRIDE;
  virtual void AbortBrowser() OVERRIDE;
  virtual bool IsBrowser(pid_t pid) OVERRIDE;
  virtual void RestartBrowserWithArgs(
      const std::vector<std::string>& args, bool args_are_extra) OVERRIDE;
  virtual void SetBrowserSessionForUser(const std::string& user) OVERRIDE;
  virtual void RunKeyGenerator() OVERRIDE;

  // Start tracking a new, potentially running key generation job.
  void AdoptKeyGeneratorJob(scoped_ptr<ChildJobInterface> job,
                            pid_t pid,
                            guint watcher);

  // Stop tracking key generation job.
  void AbandonKeyGeneratorJob();

  // Tell us that, if we want, we can cause a graceful exit from g_main_loop.
  void AllowGracefulExit();

  // |data| is a SessionManagerService*.
  static void HandleKeygenExit(GPid pid, gint status, gpointer data);

  // Ensures |args| is in the correct format, stripping "--" if needed.
  // No initial "--" is needed, but is allowed.
  // ("a", "b", "c") => ("a", "b", "c")
  // ("--", "a", "b", "c") => ("a", "b", "c").
  // Converts args from wide to plain strings.
  static std::vector<std::string> GetArgList(
      const std::vector<std::string>& args);

  // The flag to pass to chrome on a first boot.
  // Not passed when Chrome is started after signout.
  static const char kFirstBootFlag[];

  // Directory in which per-boot metrics flag files will be stored.
  static const char kFlagFileDir[];

  // Path to flag file indicating that a user has logged in since last boot.
  static const char kLoggedInFlag[];

  // Path to magic file that will trigger device wiping on next boot.
  static const char kResetFile[];

 protected:
  virtual GMainLoop* main_loop() { return main_loop_; }

 private:
  static void do_nothing(int sig) {}

  // Common code between SIG{HUP, INT, TERM}Handler.
  static void GracefulShutdownHandler(int signal);
  static void SIGHUPHandler(int signal);
  static void SIGINTHandler(int signal);
  static void SIGTERMHandler(int signal);

  // |data| is a SessionManagerService*.
  static DBusHandlerResult FilterMessage(DBusConnection* conn,
                                         DBusMessage* message,
                                         void* data);

  // |data| is a SessionManagerService*.
  static void HandleBrowserExit(GPid pid, gint status, gpointer data);

  // |data| is a SessionManagerService*.  This is a wrapper around
  // Shutdown() so that we can register it as the callback for
  // when |source| has data to read.
  static gboolean HandleKill(GIOChannel* source,
                             GIOCondition condition,
                             gpointer data);

  // Sets the proccess' exit code immediately and posts a QuitClosure to the
  // main event loop.
  void SetExitAndServiceShutdown(ExitCode code);

  // Setup any necessary signal handlers.
  void SetupHandlers();

  // Set all changed signal handlers back to the default behavior.
  void RevertHandlers();

  // Try to terminate the job represented by |spec|, but also remember it for
  // later.
  void KillAndRemember(const ChildJob::Spec& spec,
                       std::vector<std::pair<pid_t, uid_t> >* to_remember);

  // Returns appropriate child-killing timeout, depending on flag file state.
  int GetKillTimeout();

  // Terminate all children, with increasing prejudice.
  void CleanupChildren(int timeout);

  // De-register all child-exit handlers.
  void DeregisterChildWatchers();

  bool ShouldRunBrowser();
  // Returns true if |child_job| believes it should be stopped.
  // If the child believes it should be stopped (as opposed to not run anymore)
  // we actually exit the Service as well.
  bool ShouldStopChild(ChildJobInterface* child_job);

  // Run() particular ChildJobInterface, specified by |child_job|.
  int RunChild(ChildJobInterface* child_job);

  // Kill one of the children using provided signal.
  void KillChild(const ChildJobInterface* child_job, int child_pid, int signal);

  static const int kKillTimeoutCollectChrome;
  static const char kCollectChromeFile[];

  ChildJob::Spec browser_;
  ChildJob::Spec generator_;
  bool exit_on_child_done_;
  int kill_timeout_;

  gobject::SessionManager* session_manager_;
  GMainLoop* main_loop_;
  // All task posting should be done via the MessageLoopProxy in loop_proxy_.
  scoped_ptr<MessageLoop> dont_use_directly_;
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;

  SystemUtils* system_;  // Owned by the caller.
  scoped_ptr<NssUtil> nss_;
  scoped_ptr<KeyGenerator> key_gen_;
  scoped_ptr<LoginMetrics> login_metrics_;
  scoped_ptr<LivenessChecker> liveness_checker_;
  const bool enable_browser_abort_on_hang_;
  const base::TimeDelta liveness_checking_interval_;

  uid_t uid_;
  bool set_uid_;

  scoped_ptr<PolicyKey> owner_key_;

  scoped_ptr<FileChecker> file_checker_;
  scoped_ptr<OwnerKeyLossMitigator> mitigator_;

  scoped_ptr<SessionManagerInterface> impl_;

  bool shutting_down_;
  bool shutdown_already_;
  ExitCode exit_code_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerService);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
