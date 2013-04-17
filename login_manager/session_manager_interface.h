// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_
#define LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_

#include <stdlib.h>

#include <string>

#include <base/file_path.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/glib/object.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib.h>
#include <glib-object.h>

namespace login_manager {

class SessionManagerInterface {
 public:
  SessionManagerInterface() {}
  virtual ~SessionManagerInterface() {}

  // Intializes policy subsystems.  Failure to initialize must be fatal.
  virtual bool Initialize() = 0;
  virtual void Finalize() = 0;

  // Emits state change signals.
  virtual void AnnounceSessionStoppingIfNeeded() = 0;
  virtual void AnnounceSessionStopped() = 0;

  // Given a policy key stored at temp_key_file, pulls it off disk,
  // validates that it is a correctly formed key pair, and ensures it is
  // stored for the future in the provided user's NSSDB.
  virtual void ImportValidateAndStoreGeneratedKey(
      const std::string& username,
      const base::FilePath& temp_key_file) = 0;
  virtual bool ScreenIsLocked() = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Methods exposed via RPC are defined below.

  // Emits the "login-prompt-ready" and "login-prompt-visible" upstart signals.
  virtual gboolean EmitLoginPromptReady(gboolean* OUT_emitted,
                                        GError** error) = 0;
  virtual gboolean EmitLoginPromptVisible(GError** error) = 0;

  // Adds an argument to the chrome child job that makes it open a testing
  // channel, then kills and restarts chrome. The name of the socket used
  // for testing is returned in OUT_filepath.
  // If force_relaunch is true, Chrome will be restarted with each
  // invocation. Otherwise, it will only be restarted on the first invocation.
  // The extra_args parameter can include any additional arguments
  // that need to be passed to Chrome on subsequent launches.
  virtual gboolean EnableChromeTesting(gboolean force_relaunch,
                                       const gchar** extra_args,
                                       gchar** OUT_filepath,
                                       GError** error) = 0;

  // In addition to emitting "start-user-session" upstart signal and
  // "SessionStateChanged:started" D-Bus signal, this function will
  // also call browser_.job->StartSession(email_address).
  virtual gboolean StartSession(gchar* email_address,
                                gchar* unique_identifier,
                                gboolean* OUT_done,
                                GError** error) = 0;

  // In addition to emitting "stop-user-session", this function will
  // also call browser_.job->StopSession().
  virtual gboolean StopSession(gchar* unique_identifier,
                               gboolean* OUT_done,
                               GError** error) = 0;

  // |policy_blob| is a serialized protobuffer containing a device policy
  // and a signature over that policy.  Verify the sig and persist
  // |policy_blob| to disk.
  //
  // The signature is a SHA1 with RSA signature over the policy,
  // verifiable with |key_|.
  //
  // Returns TRUE if the signature checks out, FALSE otherwise.
  virtual gboolean StorePolicy(GArray* policy_blob,
                               DBusGMethodInvocation* context) = 0;

  // Get the policy_blob and associated signature off of disk.
  // Returns TRUE if the data is can be fetched, FALSE otherwise.
  virtual gboolean RetrievePolicy(GArray** OUT_policy_blob, GError** error) = 0;

  // Similar to StorePolicy above, but for user policy. |policy_blob| is a
  // serialized PolicyFetchResponse protobuf which wraps the actual policy data
  // along with an SHA1-RSA signature over the policy data. The policy data is
  // opaque to session manager, the exact definition is only relevant to client
  // code in Chrome.
  //
  // Calling this function attempts to persist |policy_blob| for the currently
  // logged in user. Policy is stored in a root-owned location within the user's
  // cryptohome (for privacy reasons). The first attempt to store policy also
  // installs the signing key for user policy. This key is used later to verify
  // policy updates pushed by Chrome.
  //
  // Returns FALSE on immediate (synchronous) errors. Otherwise, returns TRUE
  // and reports the final result of the call asynchronously through |context|.
  virtual gboolean StoreUserPolicy(GArray* policy_blob,
                                   DBusGMethodInvocation* context) = 0;

  // Retrieves user policy for the currently logged in user and returns it in
  // |policy_blob|. Returns TRUE if the policy is available, FALSE otherwise.
  virtual gboolean RetrieveUserPolicy(GArray** OUT_policy_blob,
                                      GError** error) = 0;

  // This will replace StoreUserPolicy. It adds the additional |user_email|
  // parameter.
  virtual gboolean StorePolicyForUser(gchar* user_email,
                                      GArray* policy_blob,
                                      DBusGMethodInvocation* context) = 0;

  // This wil replace RetrieveUserPolicy. It adds the additional |user_email|
  // parameter.
  virtual gboolean RetrievePolicyForUser(gchar* user_email,
                                         GArray** OUT_policy_blob,
                                         GError** error) = 0;

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
  virtual gboolean StoreDeviceLocalAccountPolicy(
      gchar* account_id,
      GArray* policy_blob,
      DBusGMethodInvocation* context) = 0;

  // Retrieves device-local account policy for the specified |account_id| and
  // returns it in |policy_blob|. Returns TRUE if the policy is available, FALSE
  // otherwise.
  virtual gboolean RetrieveDeviceLocalAccountPolicy(gchar* account_id,
                                                    GArray** OUT_policy_blob,
                                                    GError** error) = 0;

  // Get information about the current session.
  virtual gboolean RetrieveSessionState(gchar** OUT_state) = 0;

  // Handles LockScreen request from Chromium or PowerManager. It emits
  // LockScreen signal to Chromium Browser to tell it to lock the screen. The
  // browser should call the HandleScreenLocked method when the screen is
  // actually locked.
  virtual gboolean LockScreen(GError** error) = 0;

  // Intended to be called by Chromium. Updates canonical system-locked state,
  // and broadcasts ScreenIsLocked signal over DBus.
  virtual gboolean HandleLockScreenShown(GError** error) = 0;

  // Handles UnlockScreen request from Chromium. It emits UnlockScreen
  // signal to Chromium Browser to tell it to unlock the screen. The browser
  // should call the HandleScreenUnlocked method when the screen is actually
  // unlocked.
  virtual gboolean UnlockScreen(GError** error) = 0;

  // Intended to be called by Chromium. Updates canonical system-locked state,
  // and broadcasts ScreenIsUnlocked signal over DBus.
  virtual gboolean HandleLockScreenDismissed(GError** error) = 0;

  // Restarts job with specified pid replacing its command line arguments
  // with provided.
  virtual gboolean RestartJob(gint pid,
                              gchar* arguments,
                              gboolean* OUT_done,
                              GError** error) = 0;

  // Restarts job with specified pid as RestartJob(), but authenticates the
  // caller based on a supplied cookie value also known to the session manager
  // rather than pid.
  virtual gboolean RestartJobWithAuth(gint pid,
                                      gchar* cookie,
                                      gchar* arguments,
                                      gboolean* OUT_done,
                                      GError** error) = 0;

  // Sets the device up to "Powerwash" on reboot, and then triggers a reboot.
  virtual gboolean StartDeviceWipe(gboolean *OUT_done, GError** error) = 0;
};

}  // namespace login_manager
#endif  // LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_
