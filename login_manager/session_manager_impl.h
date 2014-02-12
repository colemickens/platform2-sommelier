// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_IMPL_H_
#define LOGIN_MANAGER_SESSION_MANAGER_IMPL_H_

#include <map>

#include <stdlib.h>

#include <base/basictypes.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/error_constants.h>
#include <chromeos/glib/object.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib.h>
#include <glib-object.h>

#include "login_manager/device_policy_service.h"
#include "login_manager/key_generator.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_service.h"
#include "login_manager/regen_mitigator.h"
#include "login_manager/session_manager_interface.h"

namespace login_manager {
class DBusSignalEmitterInterface;
class DeviceLocalAccountPolicyService;
class KeyGenerator;
class LoginMetrics;
class NssUtil;
class ProcessManagerServiceInterface;
class SystemUtils;
class UpstartSignalEmitter;
class UserPolicyServiceFactory;

// Friend test classes.
class SessionManagerImplStaticTest;

// Implements the DBus SessionManagerInterface.
//
// All signatures used in the methods of the ownership API are
// SHA1 with RSA encryption.
class SessionManagerImpl : public SessionManagerInterface,
                           public KeyGenerator::Delegate,
                           public PolicyService::Delegate {
 public:
  // Magic user name strings.
  static const char kDemoUser[];

  // Payloads for SessionStateChanged DBus signal.
  static const char kStarted[];
  static const char kStopping[];
  static const char kStopped[];

  // Path to flag file indicating that a user has logged in since last boot.
  static const char kLoggedInFlag[];

  // Path to magic file that will trigger device wiping on next boot.
  static const char kResetFile[];

  SessionManagerImpl(scoped_ptr<UpstartSignalEmitter> emitter,
                     DBusSignalEmitterInterface* dbus_emitter,
                     KeyGenerator* key_gen,
                     ProcessManagerServiceInterface* manager,
                     LoginMetrics* metrics,
                     NssUtil* nss,
                     SystemUtils* utils);
  virtual ~SessionManagerImpl();

  void InjectPolicyServices(
      scoped_ptr<DevicePolicyService> device_policy,
      scoped_ptr<UserPolicyServiceFactory> user_policy_factory,
      scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy);

  // SessionManagerInterface implementation.
  // Should set up policy stuff; if false DIE.
  virtual bool Initialize() OVERRIDE;
  virtual void Finalize() OVERRIDE;

  virtual void AnnounceSessionStoppingIfNeeded() OVERRIDE;
  virtual void AnnounceSessionStopped() OVERRIDE;
  virtual bool ScreenIsLocked() OVERRIDE { return screen_locked_; }
  virtual std::vector<std::string> GetStartUpFlags() OVERRIDE {
    return device_policy_->GetStartUpFlags();
  }

  // Starts a 'Powerwash' of the device by touching a flag file, then
  // rebooting to allow early-boot code to wipe parts of stateful we
  // need wiped. Have a look at /src/platform/init/chromeos_startup
  // for the gory details.
  virtual void InitiateDeviceWipe() OVERRIDE;

  //////////////////////////////////////////////////////////////////////////////
  // Methods exposed via RPC are defined below.

  // Emits the "login-prompt-ready" and "login-prompt-visible" upstart signals.
  gboolean EmitLoginPromptReady(gboolean* OUT_emitted, GError** error);
  gboolean EmitLoginPromptVisible(GError** error);

  // Adds an argument to the chrome child job that makes it open a testing
  // channel, then kills and restarts chrome. The name of the socket used
  // for testing is returned in OUT_filepath.
  // If force_relaunch is true, Chrome will be restarted with each
  // invocation. Otherwise, it will only be restarted on the first invocation.
  // The extra_args parameter can include any additional arguments
  // that need to be passed to Chrome on subsequent launches.
  gboolean EnableChromeTesting(gboolean force_relaunch,
                               const gchar** extra_args,
                               gchar** OUT_filepath,
                               GError** error);

  // In addition to emitting "start-user-session" upstart signal and
  // "SessionStateChanged:started" D-Bus signal, this function will
  // also call browser_.job->StartSession(email_address).
  gboolean StartSession(gchar* email_address,
                        gchar* unique_identifier,
                        gboolean* OUT_done,
                        GError** error);

  // In addition to emitting "stop-user-session", this function will
  // also call browser_.job->StopSession().
  gboolean StopSession(gchar* unique_identifier,
                       gboolean* OUT_done,
                       GError** error);

  // |policy_blob| is a serialized protobuffer containing a device policy
  // and a signature over that policy.  Verify the sig and persist
  // |policy_blob| to disk.
  //
  // The signature is a SHA1 with RSA signature over the policy,
  // verifiable with |key_|.
  //
  // Returns TRUE if the signature checks out, FALSE otherwise.
  gboolean StorePolicy(GArray* policy_blob, DBusGMethodInvocation* context);

  // Get the policy_blob and associated signature off of disk.
  // Returns TRUE if the data is can be fetched, FALSE otherwise.
  gboolean RetrievePolicy(GArray** OUT_policy_blob, GError** error);

  // Similar to StorePolicy above, but for user policy. |policy_blob| is a
  // serialized PolicyFetchResponse protobuf which wraps the actual policy data
  // along with an SHA1-RSA signature over the policy data. The policy data is
  // opaque to session manager, the exact definition is only relevant to client
  // code in Chrome.
  //
  // Calling this function attempts to persist |policy_blob| for |user_email|.
  // Policy is stored in a root-owned location within the user's cryptohome
  // (for privacy reasons). The first attempt to store policy also installs the
  // signing key for user policy. This key is used later to verify policy
  // updates pushed by Chrome.
  //
  // Returns FALSE on immediate (synchronous) errors. Otherwise, returns TRUE
  // and reports the final result of the call asynchronously through |context|.
  gboolean StorePolicyForUser(gchar* user_email,
                              GArray* policy_blob,
                              DBusGMethodInvocation* context);

  // Retrieves user policy for |user_email| and returns it in |policy_blob|.
  // Returns TRUE if the policy is available, FALSE otherwise.
  gboolean RetrievePolicyForUser(gchar* user_email,
                                 GArray** OUT_policy_blob,
                                 GError** error);

  // Similar to StorePolicy above, but for device-local accounts. |policy_blob|
  // is a serialized PolicyFetchResponse protobuf which wraps the actual policy
  // data along with an SHA1-RSA signature over the policy data. The policy data
  // is opaque to session manager, the exact definition is only relevant to
  // client code in Chrome.
  //
  // Calling this function attempts to persist |policy_blob| for the
  // device-local account specified in the |account_id| parameter. Policy is
  // stored in the root-owned /var/lib/device_local_accounts directory in the
  // stateful partition. Signatures are checked against the owner key, key
  // rotation is not allowed.
  //
  // Returns FALSE on immediate (synchronous) errors. Otherwise, returns TRUE
  // and reports the final result of the call asynchronously through |context|.
  gboolean StoreDeviceLocalAccountPolicy(gchar* account_id,
                                         GArray* policy_blob,
                                         DBusGMethodInvocation* context);

  // Retrieves device-local account policy for the specified |account_id| and
  // returns it in |policy_blob|. Returns TRUE if the policy is available, FALSE
  // otherwise.
  gboolean RetrieveDeviceLocalAccountPolicy(gchar* account_id,
                                            GArray** OUT_policy_blob,
                                            GError** error);

  // Get information about the current session.
  gboolean RetrieveSessionState(gchar** OUT_state);

  // Enumerate active user sessions.
  // The return value is a hash table, mapping {username: sanitized username},
  // sometimes called the "user hash".
  // The contents of the hash table are owned by the implementation of
  // this interface, but the caller is responsible for unref'ing
  // the GHashTable structure.
  GHashTable* RetrieveActiveSessions();

  // Handles LockScreen request from Chromium or PowerManager. It emits
  // LockScreen signal to Chromium Browser to tell it to lock the screen. The
  // browser should call the HandleScreenLocked method when the screen is
  // actually locked.
  gboolean LockScreen(GError** error);

  // Intended to be called by Chromium. Updates canonical system-locked state,
  // and broadcasts ScreenIsLocked signal over DBus.
  gboolean HandleLockScreenShown(GError** error);

  // Intended to be called by Chromium. Updates canonical system-locked state,
  // and broadcasts ScreenIsUnlocked signal over DBus.
  gboolean HandleLockScreenDismissed(GError** error);

  // Restarts job with specified pid replacing its command line arguments
  // with provided.
  gboolean RestartJob(gint pid,
                      gchar* arguments,
                      gboolean* OUT_done,
                      GError** error);

  // Restarts job with specified pid as RestartJob(), but authenticates the
  // caller based on a supplied cookie value also known to the session manager
  // rather than pid.
  gboolean RestartJobWithAuth(gint pid,
                              gchar* cookie,
                              gchar* arguments,
                              gboolean* OUT_done,
                              GError** error);

  // Sets the device up to "Powerwash" on reboot, and then triggers a reboot.
  gboolean StartDeviceWipe(gboolean *OUT_done, GError** error);

  // Stores in memory the flags that session manager should apply the next time
  // it restarts Chrome inside an existing session. The flags should be cleared
  // on StopSession signal or when session manager itself is restarted. Chrome
  // will wait on successful confirmation of this call and terminate itself to
  // allow session manager to restart it and apply the necessary flag. All flag
  // validation is to be done inside Chrome.
  gboolean SetFlagsForUser(gchar* user_email,
                           const gchar** flags,
                           GError** error);

  // PolicyService::Delegate implementation:
  virtual void OnPolicyPersisted(bool success) OVERRIDE;
  virtual void OnKeyPersisted(bool success) OVERRIDE;

  // KeyGenerator::Delegate implementation:
  virtual void OnKeyGenerated(const std::string& username,
                              const base::FilePath& temp_key_file) OVERRIDE;

 private:
  // Holds the state related to one of the signed in users.
  struct UserSession;

  friend class SessionManagerImplStaticTest;

  typedef std::map<std::string, UserSession*> UserSessionMap;

  // Given a policy key stored at temp_key_file, pulls it off disk,
  // validates that it is a correctly formed key pair, and ensures it is
  // stored for the future in the provided user's NSSDB.
  void ImportValidateAndStoreGeneratedKey(const std::string& username,
                                          const base::FilePath& temp_key_file);

  // Encodes the result of a policy retrieve operation as specified in |success|
  // and |policy_data| into |policy_blob| and |error|. Returns TRUE if
  // successful, FALSE otherwise.
  gboolean EncodeRetrievedPolicy(bool success,
                                 const std::vector<uint8>& policy_data,
                                 GArray** policy_blob,
                                 GError** error);

  // Safely converts a gchar* parameter from DBUS to a std::string.
  static std::string GCharToString(const gchar* str);

  // Initializes |error| with |code| and |message|.
  static void SetGError(GError** error,
                        ChromeOSLoginError code,
                        const char* message);
  // Perform very, very basic validation of |email_address|.
  static bool ValidateEmail(const std::string& email_address);

  // Check cookie against internally stored auth cookie.
  bool IsValidCookie(const char *cookie);

  bool AllSessionsAreIncognito();

  UserSession* CreateUserSession(const std::string& username,
                                 bool is_incognito,
                                 GError** error);

  PolicyService* GetPolicyService(gchar* user_email);

  bool session_started_;
  bool session_stopping_;
  bool screen_locked_;
  std::string cookie_;

  base::FilePath chrome_testing_path_;

  scoped_ptr<UpstartSignalEmitter> upstart_signal_emitter_;

  DBusSignalEmitterInterface* dbus_emitter_;  // Owned by the caller.
  KeyGenerator* key_gen_;  // Owned by the caller.
  ProcessManagerServiceInterface* manager_;  // Owned by the caller.
  LoginMetrics* login_metrics_;  // Owned by the caller.
  NssUtil* nss_;  // Owned by the caller.
  SystemUtils* system_;  // Owned by the caller.

  scoped_ptr<DevicePolicyService> device_policy_;
  scoped_ptr<UserPolicyServiceFactory> user_policy_factory_;
  scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy_;
  PolicyKey owner_key_;
  RegenMitigator mitigator_;

  // Map of the currently signed-in users to their state.
  UserSessionMap user_sessions_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerImpl);
};

}  // namespace login_manager
#endif  // LOGIN_MANAGER_SESSION_MANAGER_IMPL_H_
