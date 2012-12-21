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
class CommandLine;
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
      public login_manager::PolicyService::Delegate,
      public login_manager::SessionManagerInterface {
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
    void set_device_policy_service(DevicePolicyService* device_policy) {
      session_manager_service_->device_policy_ = device_policy;
    }
    void set_user_policy_service_factory(UserPolicyServiceFactory* factory) {
      session_manager_service_->user_policy_factory_.reset(factory);
    }
    void set_device_local_account_policy_service(
        DeviceLocalAccountPolicyService* device_local_account_policy) {
      session_manager_service_->device_local_account_policy_.reset(
          device_local_account_policy);
    }
    void set_upstart_signal_emitter(UpstartSignalEmitter* emitter) {
      session_manager_service_->upstart_signal_emitter_.reset(emitter);
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
    // Sets whether the the manager exits when a child finishes.
    void set_exit_on_child_done(bool do_exit) {
      session_manager_service_->exit_on_child_done_ = do_exit;
    }

    // Executes the CleanupChildren() method on the manager.
    void CleanupChildren(int timeout) {
      session_manager_service_->CleanupChildren(timeout);
    }

    // Set whether a session has been started.
    void set_session_started(bool started, const std::string& email) {
      session_manager_service_->session_started_ = started;
      session_manager_service_->current_user_is_incognito_ = false;
      if (started)
        session_manager_service_->current_user_ = email;
      else
        session_manager_service_->current_user_.clear();
    }

    // Set whether a Guest session has been started.
    void set_guest_session_started(bool started) {
      session_manager_service_->session_started_ = started;
      if (started) {
        session_manager_service_->current_user_ = kIncognitoUser;
        session_manager_service_->current_user_is_incognito_ = true;
      } else {
        session_manager_service_->current_user_.clear();
        session_manager_service_->current_user_is_incognito_ = false;
      }
    }

    void set_machine_info_file(const FilePath& file) {
      session_manager_service_->machine_info_file_ = file;
    }

    // Sets whether the screen is locked.
    void set_screen_locked(bool screen_locked) {
      session_manager_service_->screen_locked_ = screen_locked;
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
  virtual bool Initialize();
  virtual bool Register(const chromeos::dbus::BusConnection &conn);
  virtual bool Reset();

  // login_manager::PolicyService::Delegate implementation:
  virtual void OnPolicyPersisted(bool success);
  virtual void OnKeyPersisted(bool success);

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

  // Fork, then call browser_.job->Run() in the child and set a
  // babysitter in the parent's glib default context that calls
  // HandleBrowserExit when the child is done.
  void RunBrowser();

  // Abort the browser process.
  virtual void AbortBrowser();

  // Check if |pid| is the currently-managed browser process.
  bool IsKnownChild(pid_t pid);

  // Start tracking a new, potentially running key generation job.
  void AdoptKeyGeneratorJob(scoped_ptr<ChildJobInterface> job,
                            pid_t pid,
                            guint watcher);

  // Stop tracking key generation job.
  void AbandonKeyGeneratorJob();

  // Tell us that, if we want, we can cause a graceful exit from g_main_loop.
  void AllowGracefulExit();

  // SessionManagerInterface implementation.
  virtual gboolean EmitLoginPromptReady(gboolean* OUT_emitted,
                                        GError** error) OVERRIDE;
  virtual gboolean EmitLoginPromptVisible(GError** error) OVERRIDE;

  virtual gboolean EnableChromeTesting(gboolean force_relaunch,
                                       const gchar** extra_args,
                                       gchar** OUT_filepath,
                                       GError** error) OVERRIDE;

  virtual gboolean StartSession(gchar* email_address,
                                gchar* unique_identifier,
                                gboolean* OUT_done,
                                GError** error) OVERRIDE;
  virtual gboolean StopSession(gchar* unique_identifier,
                               gboolean* OUT_done,
                               GError** error) OVERRIDE;

  virtual gboolean StorePolicy(GArray* policy_blob,
                               DBusGMethodInvocation* context) OVERRIDE;
  virtual gboolean RetrievePolicy(GArray** OUT_policy_blob,
                                  GError** error) OVERRIDE;

  virtual gboolean StoreUserPolicy(GArray* policy_blob,
                                   DBusGMethodInvocation* context) OVERRIDE;
  virtual gboolean RetrieveUserPolicy(GArray** OUT_policy_blob,
                                      GError** error) OVERRIDE;

  virtual gboolean StoreDeviceLocalAccountPolicy(
      gchar* account_id,
      GArray* policy_blob,
      DBusGMethodInvocation* context) OVERRIDE;
  virtual gboolean RetrieveDeviceLocalAccountPolicy(gchar* account_id,
                                                    GArray** OUT_policy_blob,
                                                    GError** error) OVERRIDE;

  virtual gboolean RetrieveSessionState(gchar** OUT_state,
                                        gchar** OUT_user) OVERRIDE;

  virtual gboolean LockScreen(GError** error) OVERRIDE;
  virtual gboolean HandleLockScreenShown(GError** error) OVERRIDE;
  virtual gboolean UnlockScreen(GError** error) OVERRIDE;
  virtual gboolean HandleLockScreenDismissed(GError** error) OVERRIDE;

  virtual gboolean RestartJob(gint pid,
                              gchar* arguments,
                              gboolean* OUT_done,
                              GError** error) OVERRIDE;
  virtual gboolean RestartJobWithAuth(gint pid,
                                      gchar* cookie,
                                      gchar* arguments,
                                      gboolean* OUT_done,
                                      GError** error) OVERRIDE;

  virtual gboolean StartDeviceWipe(gboolean *OUT_done, GError** error) OVERRIDE;

  // |data| is a SessionManagerService*.
  static void HandleKeygenExit(GPid pid, gint status, gpointer data);

  // Perform very, very basic validation of |email_address|.
  static bool ValidateEmail(const std::string& email_address);

  // Ensures |args| is in the correct format, stripping "--" if needed.
  // No initial "--" is needed, but is allowed.
  // ("a", "b", "c") => ("a", "b", "c")
  // ("--", "a", "b", "c") => ("a", "b", "c").
  // Converts args from wide to plain strings.
  static std::vector<std::string> GetArgList(
      const std::vector<std::string>& args);

  // Safely converts a gchar* parameter from DBUS to a std::string.
  static std::string GCharToString(const gchar* str);

  // The flag to pass to chrome to open a named socket for testing.
  static const char kTestingChannelFlag[];

  // The flag to pass to chrome on a first boot.
  // Not passed when Chrome is started after signout.
  static const char kFirstBootFlag[];

  // Payloads for SessionStateChanged DBus signal.
  static const char kStarted[];
  static const char kStopping[];
  static const char kStopped[];

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

  // Cache |email_address| in |current_user_| and return true, if the address
  // passes validation.  Otherwise, set |error| appropriately and return false.
  gboolean ValidateAndCacheUserEmail(const gchar* email_address,
                                     GError** error);

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

  // |policy_| is persisted to disk, then |event| is signaled when
  // done.  This is used to provide synchronous, threadsafe persisting.
  void PersistPolicySync(base::WaitableEvent* event);

  // Uses |system_| to send |signal_name| to Chromium.  Attaches a payload
  // to the signal indicating the status of |succeeded|.
  void SendSignal(const char signal_name[], bool succeeded);

  bool ShouldRunBrowser();
  // Returns true if |child_job| believes it should be stopped.
  // If the child believes it should be stopped (as opposed to not run anymore)
  // we actually exit the Service as well.
  bool ShouldStopChild(ChildJobInterface* child_job);

  // Run() particular ChildJobInterface, specified by |child_job|.
  int RunChild(ChildJobInterface* child_job);

  // Kill one of the children using provided signal.
  void KillChild(const ChildJobInterface* child_job, int child_pid, int signal);

  // Encodes the result of a policy retrieve operation as specified in |success|
  // and |policy_data| into |policy_blob| and |error|. Returns TRUE if
  // successful, FALSE otherwise.
  gboolean EncodeRetrievedPolicy(bool success,
                                 const std::vector<uint8>& policy_data,
                                 GArray** policy_blob,
                                 GError** error);

  // Starts a 'Powerwash' of the device by touching a flag file, then
  // rebooting to allow early-boot code to wipe parts of stateful we
  // need wiped. Have a look at /src/platform/init/chromeos_startup
  // for the gory details.
  void InitiateDeviceWipe();

  bool IsValidCookie(const char *cookie);

  static const uint32 kMaxGCharBufferSize;
  static const char kEmailSeparator;
  static const char kLegalCharacters[];
  static const char kIncognitoUser[];
  static const char kDemoUser[];
  static const char kLoginUserTypeMetric[];
  // The name of the pref that Chrome sets to track who the owner is.
  static const char kDeviceOwnerPref[];

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
  scoped_ptr<UpstartSignalEmitter> upstart_signal_emitter_;
  scoped_ptr<LivenessChecker> liveness_checker_;
  const bool enable_browser_abort_on_hang_;
  const base::TimeDelta liveness_checking_interval_;

  bool session_started_;
  bool session_stopping_;
  bool current_user_is_incognito_;
  std::string current_user_;
  bool screen_locked_;
  uid_t uid_;
  bool set_uid_;

  FilePath chrome_testing_path_;
  FilePath machine_info_file_;

  scoped_ptr<PolicyKey> owner_key_;
  scoped_refptr<DevicePolicyService> device_policy_;
  scoped_ptr<UserPolicyServiceFactory> user_policy_factory_;
  scoped_refptr<PolicyService> user_policy_;
  scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy_;

  scoped_ptr<FileChecker> file_checker_;
  scoped_ptr<OwnerKeyLossMitigator> mitigator_;

  bool shutting_down_;
  bool shutdown_already_;
  ExitCode exit_code_;

  static size_t kCookieEntropyBytes;
  std::string cookie_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerService);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
