// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
#define LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_

#include <dbus/dbus.h>
#include <errno.h>
#include <glib.h>
#include <signal.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/callback_forward.h>
#include <base/file_path.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/synchronization/waitable_event.h>
#include <base/time.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

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

class BrowserJobInterface;
class GeneratorJobInterface;
class NssUtil;
class PolicyService;
class SystemUtils;

// Provides methods for running the browser, watching its progress, and
// restarting it if need be.
//
// Once the browser is run, it will be restarted perpetually, UNLESS
// |magic_chrome_file| exists, or this process receives a termination signal.
// Also provides a wrapper that exports SessionManagerImpl methods via
// D-Bus.
//
// ::g_type_init() must be called before this class is used.
class SessionManagerService
    : public base::RefCountedThreadSafe<SessionManagerService>,
      public chromeos::dbus::AbstractDbusService,
      public login_manager::ProcessManagerServiceInterface {
 public:
  enum ExitCode {
    SUCCESS = 0,
    CRASH_WHILE_SCREEN_LOCKED = 1,
    CHILD_EXITING_TOO_FAST = 2,
    MUST_WIPE_DEVICE = 3,
  };

  SessionManagerService(scoped_ptr<BrowserJobInterface> child_job,
                        const base::Closure& quit_closure,
                        int kill_timeout,
                        bool enable_browser_abort_on_hang,
                        base::TimeDelta hang_detection_interval,
                        SystemUtils* system);
  virtual ~SessionManagerService();

  // If you want to call any of these setters, you should do so before calling
  // any other methods on this class.
  class TestApi {
   public:
    void set_systemutils(SystemUtils* utils) {
      session_manager_service_->system_ = utils;
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
    void CleanupChildren(int timeout_sec) {
      session_manager_service_->CleanupChildren(
          base::TimeDelta::FromSeconds(timeout_sec));
    }

    // Cause handling of faked-out exit of a child process.
    void ScheduleChildExit(pid_t pid, int status);

    // Trigger and handle SessionManagerImpl initialization.
    bool InitializeImpl() { return session_manager_service_->InitializeImpl(); }

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

  virtual bool Initialize() OVERRIDE;
  virtual bool Register(const chromeos::dbus::BusConnection &conn) OVERRIDE;
  virtual bool Reset() OVERRIDE;

  // DEPRECATED
  // TODO(cmasone): Remove as part of http://crbug.com/330880
  virtual bool Run() OVERRIDE;

  // DEPRECATED
  // TODO(cmasone): Remove as part of http://crbug.com/330880
  virtual bool Shutdown() OVERRIDE;

  // Tears down object set up during Initialize(), cleans up child processes,
  // and announces that the user session has stopped over DBus.
  void Finalize();

  // Takes ownership of |file_checker|.
  void set_file_checker(FileChecker* file_checker) {
    file_checker_.reset(file_checker);
  }

  // Can't be "unset".
  void set_uid(uid_t uid) {
    uid_ = uid;
    set_uid_ = true;
  }

  ExitCode exit_code() { return exit_code_; }

  // Implementing ProcessManagerServiceInterface
  virtual void ScheduleShutdown() OVERRIDE;
  virtual bool ShouldRunBrowser() OVERRIDE;
  virtual void RunBrowser() OVERRIDE;
  virtual void AbortBrowser(int signal, const std::string& message) OVERRIDE;
  virtual void RestartBrowserWithArgs(
      const std::vector<std::string>& args, bool args_are_extra) OVERRIDE;
  virtual void SetBrowserSessionForUser(const std::string& username,
                                        const std::string& userhash) OVERRIDE;
  virtual void SetFlagsForUser(const std::string& username,
                               const std::vector<std::string>& flags) OVERRIDE;
  virtual void RunKeyGenerator(const std::string& username) OVERRIDE;
  virtual void AdoptKeyGeneratorJob(scoped_ptr<GeneratorJobInterface> job,
                                    pid_t pid) OVERRIDE;
  virtual void AbandonKeyGeneratorJob() OVERRIDE;
  virtual void ProcessNewOwnerKey(const std::string& username,
                                  const base::FilePath& key_file) OVERRIDE;
  virtual bool IsBrowser(pid_t pid) OVERRIDE;
  virtual bool IsGenerator(pid_t pid) OVERRIDE;
  virtual bool IsManagedProcess(pid_t pid) OVERRIDE;

  // Ensures |args| is in the correct format, stripping "--" if needed.
  // No initial "--" is needed, but is allowed.
  // ("a", "b", "c") => ("a", "b", "c")
  // ("--", "a", "b", "c") => ("a", "b", "c").
  // Converts args from wide to plain strings.
  static std::vector<std::string> GetArgList(
      const std::vector<std::string>& args);

  // Set all changed signal handlers back to the default behavior.
  static void RevertHandlers();

  // Directory in which per-boot metrics flag files will be stored.
  static const char kFlagFileDir[];

  // Path to flag file indicating that a user has logged in since last boot.
  static const char kLoggedInFlag[];

  // Path to magic file that will trigger device wiping on next boot.
  static const char kResetFile[];

 protected:
  virtual GMainLoop* main_loop() { return main_loop_; }

 private:
  // |data| is a SessionManagerService*.
  static DBusHandlerResult FilterMessage(DBusConnection* conn,
                                         DBusMessage* message,
                                         void* data);

  // |data| is a SessionManagerService*.
  static gboolean HandleChildrenExiting(GIOChannel* source,
                                        GIOCondition condition,
                                        gpointer data);

  // |data| is a SessionManagerService*.
  static gboolean HandleKill(GIOChannel* source,
                             GIOCondition condition,
                             gpointer data);

  // Re-runs the browser, unless one of the following is true:
  //  The screen is supposed to be locked,
  //  UI shutdown is in progress,
  //  The child indicates that it should not run anymore, or
  //  ShouldRunBrowser() indicates the browser should not run anymore.
  void HandleBrowserExit();

  // Determines which child exited, and handles appropriately.
  void HandleChildExit(const siginfo_t& info);

  // Set up any necessary signal handlers.
  void SetUpHandlers();

  // Returns appropriate child-killing timeout, depending on flag file state.
  base::TimeDelta GetKillTimeout();

  // Initializes policy subsystems which, among other things, finds and
  // validates the stored policy signing key if one is present.
  // A corrupted policy key means that the device needs to have its data wiped.
  // We trigger a reboot and then wipe (most of) the stateful partition.
  bool InitializeImpl();

  // Tell us that, if we want, we can cause a graceful exit from g_main_loop.
  void AllowGracefulExit();

  // Sets the proccess' exit code immediately and posts a QuitClosure to the
  // main event loop.
  void SetExitAndScheduleShutdown(ExitCode code);

  // Terminate all children, with increasing prejudice.
  void CleanupChildren(base::TimeDelta timeout);

  // Waits |timeout| for job defined by |spec| to go away, then
  // aborts it if it's not gone.
  void WaitAndAbortChild(ChildJobInterface* job, base::TimeDelta timeout);

  static const int kKillTimeoutCollectChrome;
  static const char kCollectChromeFile[];

  scoped_ptr<BrowserJobInterface> browser_;
  scoped_ptr<GeneratorJobInterface> generator_;
  bool exit_on_child_done_;
  const base::TimeDelta kill_timeout_;

  gobject::SessionManager* session_manager_;
  GMainLoop* main_loop_;
  // All task posting should be done via the MessageLoopProxy in loop_proxy_.
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;
  base::Closure quit_closure_;

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

  scoped_ptr<SessionManagerInterface> impl_;
  scoped_refptr<DevicePolicyService> device_policy_;

  bool shutting_down_;
  bool shutdown_already_;
  ExitCode exit_code_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerService);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
