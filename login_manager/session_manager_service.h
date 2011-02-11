// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
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
#include <base/scoped_ptr.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

#include "login_manager/file_checker.h"
#include "login_manager/owner_key.h"
#include "login_manager/owner_key_loss_mitigator.h"
#include "login_manager/pref_store.h"
#include "login_manager/system_utils.h"
#include "login_manager/upstart_signal_emitter.h"

namespace login_manager {
namespace gobject {
struct SessionManager;
}  // namespace gobject

class ChildJobInterface;
class CommandLine;
class NssUtil;

// Provides a wrapper for exporting SessionManagerInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
//
// All signatures used in the methods of the ownership API are
// SHA1 with RSA encryption.
class SessionManagerService : public chromeos::dbus::AbstractDbusService {
 public:
  struct PersistKeyData {
   public:
    PersistKeyData(SystemUtils* utils, OwnerKey* key)
        : signaler(utils),
          to_persist(key) {
    }
    ~PersistKeyData() {}
    SystemUtils* signaler;
    OwnerKey* to_persist;
  };

  struct PersistStoreData {
   public:
    PersistStoreData(SystemUtils* utils, PrefStore* store)
        : signaler(utils),
          to_persist(store) {
    }
    ~PersistStoreData() {}
    SystemUtils* signaler;
    PrefStore* to_persist;
  };

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

    void set_systemutils(SystemUtils* utils) {
      session_manager_service_->system_.reset(utils);
    }
    void set_ownerkey(OwnerKey* key) {
      session_manager_service_->key_.reset(key);
    }
    void set_prefstore(PrefStore* key) {
      session_manager_service_->store_.reset(key);
    }
    void set_upstart_signal_emitter(UpstartSignalEmitter* emitter) {
      session_manager_service_->upstart_signal_emitter_.reset(emitter);
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
      if (started)
        session_manager_service_->current_user_ = email;
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
  virtual bool Register(const chromeos::dbus::BusConnection &conn);
  virtual bool Reset();

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

  // Kill one of the children.
  void KillChild(const ChildJobInterface* child_job, int child_pid);

  bool IsKnownChild(int pid);

  // Tell us that, if we want, we can cause a graceful exit from g_main_loop.
  void AllowGracefulExit();

  ////////////////////////////////////////////////////////////////////////////
  // SessionManagerService commands

  // Emits the "login-prompt-ready" and "login-prompt-visible" upstart signals.
  gboolean EmitLoginPromptReady(gboolean* OUT_emitted, GError** error);
  gboolean EmitLoginPromptVisible(GError** error);

  // Adds an argument to the chrome child job that makes it open a testing
  // channel, then kills and restarts chrome. The name of the socket used
  // for testing is returned in OUT_filepath.
  // If force_relaunch is true, Chrome will be restarted with each
  // invocation. Otherwise, it will only be restarted on the first invocation.
  // The extra_arguments parameter can include any additional arguments
  // that need to be passed to Chrome on subsequent launches.
  gboolean EnableChromeTesting(gboolean force_relaunch,
                               gchar** extra_arguments,
                               gchar** OUT_filepath,
                               GError** error);

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

  // If no owner key is currently set, uses the DER-encoded public key
  // provided in |public_key_der| as the key belonging to the owner of
  // this device.  All future requests to Whitelist an email address
  // or to store a property MUST be digitally signed by the private
  // key associated with this public key.
  // Returns TRUE if the key is accepted and successfully recorded.
  // If the key is rejected, because we already have one or for any other
  // reason, we return FALSE and set |error| appropriately.
  gboolean SetOwnerKey(GArray* public_key_der, GError** error);

  // Remove the user |email_address| from the whitelist.  |signature| must
  // be a signature over |email_address| that validates with |key_|.
  // |signature| is a binary blob representing a sha1WithRSAEncryption sig.
  gboolean Unwhitelist(gchar* email_address, GArray* signature, GError** error);

  // If |email_address| is whitelisted, return true and put the signature blob
  // in |OUT_signature|.  Otherwise, return false and set |error|.
  gboolean CheckWhitelist(gchar* email_address,
                          GArray** OUT_signature,
                          GError** error);

  // Return all email addresses on the whitelist in |OUT_whitelist| and return
  // true.
  gboolean EnumerateWhitelisted(gchar*** OUT_whitelist, GError** error);

  // Add the user |email_address| to the whitelist.  |signature| must
  // be a signature over |email_address| that validates with |key_|.
  // |signature| should a binary blob representing a sha1WithRSAEncryption sig.
  gboolean Whitelist(gchar* email_address, GArray* signature, GError** error);

  // Store |name| = |value,signature| in |store_|.  Passing '.' characters in
  // the name is fine and will create nested Dictionaries in the underlying
  // representation, though there's no way to get those dictionaries out right
  // now.
  //
  // |signature| is a SAH1 with RSA signature over the concatenation
  // of name and value, verifiable with |key_|.
  //
  // Returns TRUE if the signature checks out and the data is inserted,
  // FALSE otherwise.
  gboolean StoreProperty(gchar* name,
                         gchar* value,
                         GArray* signature,
                         GError** error);

  // Get the value and signature associated with |name| out of |store|.
  // Returns TRUE if the data is can be fetched, FALSE otherwise.
  gboolean RetrieveProperty(gchar* name,
                            gchar** OUT_value,
                            GArray** OUT_signature,
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

  // Restarts (or starts if stopped) the entd upstart job.  Returns if
  // start was successful.
  gboolean RestartEntd(GError** error);

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

  // The flag to pass to chrome to open a named socket for testing.
  static const char kTestingChannelFlag[];

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
  static DBusHandlerResult FilterMessage(DBusConnection* conn,
                                         DBusMessage* message,
                                         void* data);

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

  // |data| is a PersistKeyData*
  // data->to_persist is persisted to disk, and then
  // data->signaler is used to signal Chromium that this has occurred.
  static gboolean PersistKey(gpointer data);

  // |data| is a PersistStoreData*
  // data->to_persist is persisted to disk, and then
  // data->signaler is used to signal Chromium that this has occurred.
  static gboolean PersistWhitelist(gpointer data);

  // |data| is a PersistStoreData*
  // data->to_persist is persisted to disk, and then
  // data->signaler is used to signal Chromium that this has occurred.
  static gboolean PersistStore(gpointer data);

  // Initializes |error| with |code| and |message|.
  static void SetGError(GError** error,
                        ChromeOSLoginError code,
                        const char* message);

  // Setup any necessary signal handlers.
  void SetupHandlers();

  // Returns true if the current user is listed in |store_| as the
  // kDeviceOwner.  Returns false if not, or if that cannot be determined.
  // |error| is set appropriately on failure.
  gboolean CurrentUserIsOwner(GError** error);

  // Returns true if the current user has the private half of |pub_key|
  // in his nssdb.  Returns false if not, or if that cannot be determined.
  // |error| is set appropriately on failure.
  gboolean CurrentUserHasOwnerKey(const std::vector<uint8>& pub_key,
                                  GError** error);

  // Cache |email_address| in |current_user_| and return true, if the address
  // passes validation.  Otherwise, set |error| appropriately and return false.
  gboolean ValidateAndCacheUserEmail(const gchar* email_address,
                                     GError** error);

  // Terminate all children, with increasing prejudice.
  void CleanupChildren(int timeout);

  // If the current user has access to the owner private key
  // (read: is the owner), this call whitelists |current_user_|, sets a
  // property indicating |current_user_| is the owner, and schedules both
  // a PersistWhitelist() and a PersistStore().
  // Returns false on failure, with |error| set appropriately.
  gboolean StoreOwnerProperties(GError** error);

  // Signs and stores |name|=|value|, and schedules a PersistStore().
  // Returns false on failure, populating |error| with |err_msg|.
  gboolean SignAndStoreProperty(const std::string& name,
                                const std::string& value,
                                const std::string& err_msg,
                                GError** error);

  // Signs and whitelists |email|, and schedules a PersistWhitelist().
  // Returns false on failure, populating |error| with |err_msg|.
  gboolean SignAndWhitelist(const std::string& email,
                            const std::string& err_msg,
                            GError** error);

  // Encodes |signature| for writing to disk, stores |name|=|value|, and
  // schedules a PersistStore().
  // Returns false on failure, with |error| set appropriately.
  gboolean SetPropertyHelper(const std::string& name,
                             const std::string& value,
                             const std::string& signature,
                             GError** error);

  // Encodes |signature| for writing to disk, whitelists |email|, and
  // schedules a PersistWhitelist().
  // Returns false on failure, with |error| set appropriately.
  gboolean WhitelistHelper(const std::string& email,
                           const std::string& signature,
                           GError** error);

  // Looks for |name| in |store_|.  If there, returns the associated value
  // and the (base64-decoded) signature in the associated OUT_ params.
  // Upon failure, FALSE is returned and error is set appropriately.
  gboolean GetPropertyHelper(const std::string& name,
                             std::string* OUT_value,
                             std::string* OUT_signature,
                             GError** error);

  bool ShouldRunChildren();
  // Returns true if |child_job| believes it should be stopped.
  // If the child believes it should be stopped (as opposed to not run anymore)
  // we actually exit the Service as well.
  bool ShouldStopChild(ChildJobInterface* child_job);

  static const uint32 kMaxEmailSize;
  static const char kEmailSeparator;
  static const char kLegalCharacters[];
  static const char kIncognitoUser[];
  // The name of the pref that Chrome sets to track who the owner is.
  static const char kDeviceOwnerPref[];

  std::vector<ChildJobInterface*> child_jobs_;
  std::vector<int> child_pids_;
  bool exit_on_child_done_;

  gobject::SessionManager* session_manager_;
  GMainLoop* main_loop_;

  scoped_ptr<SystemUtils> system_;
  scoped_ptr<NssUtil> nss_;
  scoped_ptr<OwnerKey> key_;
  scoped_ptr<PrefStore> store_;
  scoped_ptr<UpstartSignalEmitter> upstart_signal_emitter_;

  bool session_started_;
  std::string current_user_;
  std::string chrome_testing_path_;

  // D-Bus GLib signal ids.
  guint signals_[kNumSignals];

  scoped_ptr<FileChecker> file_checker_;
  scoped_ptr<OwnerKeyLossMitigator> mitigator_;
  bool screen_locked_;
  uid_t uid_;
  bool set_uid_;

  bool shutting_down_;

  friend class TestAPI;
  DISALLOW_COPY_AND_ASSIGN(SessionManagerService);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SESSION_MANAGER_SERVICE_H_
