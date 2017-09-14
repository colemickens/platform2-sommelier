// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_AUTHPOLICY_H_
#define AUTHPOLICY_AUTHPOLICY_H_

#include <memory>
#include <string>
#include <vector>

#include <brillo/dbus/async_event_sequencer.h>
#include <dbus/object_proxy.h>

#include "authpolicy/authpolicy_metrics.h"
#include "authpolicy/org.chromium.AuthPolicy.h"
#include "authpolicy/samba_interface.h"

using brillo::dbus_utils::AsyncEventSequencer;

namespace authpolicy {

class ActiveDirectoryAccountInfo;
class Anonymizer;
class AuthPolicyMetrics;
class PathService;

extern const char kChromeUserPolicyType[];
extern const char kChromeDevicePolicyType[];

// Implementation of authpolicy's D-Bus interface. Mainly routes stuff between
// D-Bus and SambaInterface.
class AuthPolicy : public org::chromium::AuthPolicyAdaptor,
                   public org::chromium::AuthPolicyInterface {
 public:
  using PolicyResponseCallback =
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>>;

  // Helper method to get the D-Bus object for the given |object_manager|.
  static std::unique_ptr<brillo::dbus_utils::DBusObject> GetDBusObject(
      brillo::dbus_utils::ExportedObjectManager* object_manager);

  AuthPolicy(AuthPolicyMetrics* metrics, const PathService* path_service);

  // Initializes internals. See SambaInterface::Initialize() for details.
  ErrorType Initialize(bool expect_config);

  // Registers the D-Bus object and interfaces.
  void RegisterAsync(
      std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
      const AsyncEventSequencer::CompletionAction& completion_callback);

  // Cleans all persistent state files. Returns true if all files were cleared.
  static bool CleanState(const PathService* path_service) {
    return SambaInterface::CleanState(path_service);
  }

  // org::chromium::AuthPolicyInterface: (see org.chromium.AuthPolicy.xml).
  // |account_info_blob| is a serialized ActiveDirectoryAccountInfo protobuf.
  void AuthenticateUser(const std::string& user_principal_name,
                        const std::string& account_id,
                        const dbus::FileDescriptor& password_fd,
                        int32_t* error,
                        std::vector<uint8_t>* account_info_blob) override;

  void GetUserStatus(const std::string& account_id,
                     int32_t* error,
                     std::vector<uint8_t>* user_status_blob) override;

  // |kerberos_files_blob| is a serialized KerberosFiles profobuf.
  void GetUserKerberosFiles(const std::string& account_id,
                            int32_t* error,
                            std::vector<uint8_t>* kerberos_files_blob) override;

  int32_t JoinADDomain(const std::string& machine_name,
                       const std::string& user_principal_name,
                       const dbus::FileDescriptor& password_fd) override;

  void RefreshUserPolicy(PolicyResponseCallback callback,
                         const std::string& account_id_key) override;

  void RefreshDevicePolicy(PolicyResponseCallback callback) override;

  std::string SetDefaultLogLevel(int32_t level) override;

  // Disable retry sleep for unit tests.
  void DisableRetrySleepForTesting() { samba_.DisableRetrySleepForTesting(); }

  // Returns the anonymizer.
  const Anonymizer* GetAnonymizerForTesting() const {
    return samba_.GetAnonymizerForTesting();
  }

  // Renew the user ticket-granting-ticket.
  ErrorType RenewUserTgtForTesting() { return samba_.RenewUserTgtForTesting(); }

 private:
  // Gets triggered by when the Kerberos credential cache or the configuration
  // file of the currently logged in user change. Triggers the
  // UserKerberosFilesChanged signal.
  void OnUserKerberosFilesChanged();

  // Sends policy to SessionManager. Assumes |gpo_policy_data| contains user
  // policy if |account_id_key| is not nullptr, otherwise assumes it's device
  // policy.
  void StorePolicy(const protos::GpoPolicyData& gpo_policy_data,
                   const std::string* account_id_key,
                   std::unique_ptr<ScopedTimerReporter> timer,
                   PolicyResponseCallback callback);

  // Response callback from SessionManager, logs the result and calls callback.
  void OnPolicyStored(bool is_user_policy,
                      std::unique_ptr<ScopedTimerReporter> timer,
                      PolicyResponseCallback callback,
                      dbus::Response* response);

  AuthPolicyMetrics* metrics_;  // Not owned.
  SambaInterface samba_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  dbus::ObjectProxy* session_manager_proxy_ = nullptr;
  base::WeakPtrFactory<AuthPolicy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthPolicy);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_AUTHPOLICY_H_
