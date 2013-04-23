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
#include "login_manager/policy_service.h"
#include "login_manager/session_manager_interface.h"

namespace login_manager {
class DeviceLocalAccountPolicyService;
class LoginMetrics;
class PolicyKey;
class ProcessManagerServiceInterface;
class SystemUtils;
class UpstartSignalEmitter;
class UserPolicyServiceFactory;

// Friend test classes.
class SessionManagerImplStaticTest;

class SessionManagerImpl : public SessionManagerInterface,
                           public login_manager::PolicyService::Delegate {
 public:
  SessionManagerImpl(scoped_ptr<UpstartSignalEmitter> emitter,
                     ProcessManagerServiceInterface* manager,
                     LoginMetrics* metrics,
                     SystemUtils* utils);
  virtual ~SessionManagerImpl();

  void InjectPolicyServices(
      const scoped_refptr<DevicePolicyService>& device_policy,
      scoped_ptr<UserPolicyServiceFactory> user_policy_factory,
      scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy);

  // SessionManagerInterface implementation.
  void AnnounceSessionStoppingIfNeeded() OVERRIDE;
  void AnnounceSessionStopped() OVERRIDE;
  void ImportValidateAndStoreGeneratedKey(const FilePath& temp_key_file);
  bool ScreenIsLocked() OVERRIDE { return screen_locked_; }
  // Should set up policy stuff; if false DIE.
  bool Initialize() OVERRIDE;
  void Finalize() OVERRIDE;

  void ValidateAndStoreOwnerKey(const std::string& key);

  gboolean EmitLoginPromptReady(gboolean* OUT_emitted,
                                GError** error) OVERRIDE;
  gboolean EmitLoginPromptVisible(GError** error) OVERRIDE;

  gboolean EnableChromeTesting(gboolean force_relaunch,
                               const gchar** extra_args,
                               gchar** OUT_filepath,
                               GError** error) OVERRIDE;

  gboolean StartSession(gchar* email_address,
                        gchar* unique_identifier,
                        gboolean* OUT_done,
                        GError** error) OVERRIDE;
  gboolean StopSession(gchar* unique_identifier,
                       gboolean* OUT_done,
                       GError** error) OVERRIDE;

  gboolean StorePolicy(GArray* policy_blob,
                       DBusGMethodInvocation* context) OVERRIDE;
  gboolean RetrievePolicy(GArray** OUT_policy_blob, GError** error) OVERRIDE;

  gboolean StoreUserPolicy(GArray* policy_blob,
                           DBusGMethodInvocation* context) OVERRIDE;
  gboolean RetrieveUserPolicy(GArray** OUT_policy_blob,
                              GError** error) OVERRIDE;

  gboolean StorePolicyForUser(gchar* user_email,
                              GArray* policy_blob,
                              DBusGMethodInvocation* context) OVERRIDE;
  gboolean RetrievePolicyForUser(gchar* user_email,
                                 GArray** OUT_policy_blob,
                                 GError** error) OVERRIDE;

  gboolean StoreDeviceLocalAccountPolicy(
      gchar* account_id,
      GArray* policy_blob,
      DBusGMethodInvocation* context) OVERRIDE;
  gboolean RetrieveDeviceLocalAccountPolicy(gchar* account_id,
                                            GArray** OUT_policy_blob,
                                            GError** error) OVERRIDE;

  gboolean RetrieveSessionState(gchar** OUT_state) OVERRIDE;

  gboolean LockScreen(GError** error) OVERRIDE;
  gboolean HandleLockScreenShown(GError** error) OVERRIDE;

  gboolean UnlockScreen(GError** error) OVERRIDE;
  gboolean HandleLockScreenDismissed(GError** error) OVERRIDE;

  gboolean RestartJob(gint pid,
                      gchar* arguments,
                      gboolean* OUT_done,
                      GError** error) OVERRIDE;
  gboolean RestartJobWithAuth(gint pid,
                              gchar* cookie,
                              gchar* arguments,
                              gboolean* OUT_done,
                              GError** error) OVERRIDE;

  gboolean StartDeviceWipe(gboolean *OUT_done, GError** error) OVERRIDE;

  // login_manager::PolicyService::Delegate implementation:
  virtual void OnPolicyPersisted(bool success) OVERRIDE;
  virtual void OnKeyPersisted(bool success) OVERRIDE;

  // Magic user name strings.
  static const char kIncognitoUser[];
  static const char kDemoUser[];

  // Payloads for SessionStateChanged DBus signal.
  static const char kStarted[];
  static const char kStopping[];
  static const char kStopped[];

  // Path to flag file indicating that a user has logged in since last boot.
  static const char kLoggedInFlag[];

  // Path to magic file that will trigger device wiping on next boot.
  static const char kResetFile[];

 private:
  // Holds the state related to one of the signed in users.
  struct UserSession;

  friend class SessionManagerImplStaticTest;

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

  UserSession* CreateUserSession(const std::string& username,
                                 bool is_incognito,
                                 GError** error);

  scoped_refptr<PolicyService> GetPolicyService(gchar* user_email);

  bool session_started_;
  bool session_stopping_;
  bool screen_locked_;
  std::string cookie_;

  base::FilePath chrome_testing_path_;

  scoped_ptr<UpstartSignalEmitter> upstart_signal_emitter_;

  ProcessManagerServiceInterface* manager_;  // Owned by the caller.
  LoginMetrics* login_metrics_;  // Owned by the caller.
  SystemUtils* system_;  // Owned by the caller.

  scoped_refptr<DevicePolicyService> device_policy_;
  scoped_ptr<UserPolicyServiceFactory> user_policy_factory_;
  scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy_;

  // Map of the currently signed-in users to their state.
  std::map<std::string, UserSession*> user_sessions_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerImpl);
};

}  // namespace login_manager
#endif  // LOGIN_MANAGER_SESSION_MANAGER_IMPL_H_
