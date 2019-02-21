// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_SERVICE_USERDATAAUTH_H_
#define CRYPTOHOME_SERVICE_USERDATAAUTH_H_

#include <memory>

#include <dbus/cryptohome/dbus-constants.h>
#include <cryptohome/proto_bindings/UserDataAuth.pb.h>

#include "dbus_adaptors/org.chromium.UserDataAuth.h"

namespace cryptohome {
class UserDataAuthAdaptor
    : public org::chromium::UserDataAuthInterfaceInterface,
      public org::chromium::UserDataAuthInterfaceAdaptor {
 public:
  explicit UserDataAuthAdaptor(scoped_refptr<dbus::Bus> bus)
      : org::chromium::UserDataAuthInterfaceAdaptor(this),
        dbus_object_(nullptr, bus, dbus::ObjectPath(kUserDataAuthServicePath)) {
  }

  void RegisterAsync(
      const brillo::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback) {
    // completion_callback is a callback that will be run when all method
    // registration have finished. We don't have anything to run after
    // completion so we'll just pass this along to libbrillo.
    // This callback is typically used to signal to the DBus Daemon that method
    // registration is complete
    RegisterWithDBusObject(&dbus_object_);
    dbus_object_.RegisterAsync(completion_callback);
  }

  // Interface overrides
  void IsMounted(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                     user_data_auth::IsMountedReply>> response,
                 const user_data_auth::IsMountedRequest& in_request) override;
  void Unmount(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                   user_data_auth::UnmountReply>> response,
               const user_data_auth::UnmountRequest& in_request) override;
  void Mount(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                 user_data_auth::MountReply>> response,
             const user_data_auth::MountRequest& in_request) override;
  void Remove(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                  user_data_auth::RemoveReply>> response,
              const user_data_auth::RemoveRequest& in_request) override;
  void Rename(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                  user_data_auth::RenameReply>> response,
              const user_data_auth::RenameRequest& in_request) override;
  void ListKeys(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                    user_data_auth::ListKeysReply>> response,
                const user_data_auth::ListKeysRequest& in_request) override;
  void GetKeyData(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                      user_data_auth::GetKeyDataReply>> response,
                  const user_data_auth::GetKeyDataRequest& in_request) override;
  void CheckKey(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                    user_data_auth::CheckKeyReply>> response,
                const user_data_auth::CheckKeyRequest& in_request) override;
  void AddKey(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                  user_data_auth::AddKeyReply>> response,
              const user_data_auth::AddKeyRequest& in_request) override;
  void UpdateKey(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                     user_data_auth::UpdateKeyReply>> response,
                 const user_data_auth::UpdateKeyRequest& in_request) override;
  void RemoveKey(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                     user_data_auth::RemoveKeyReply>> response,
                 const user_data_auth::RemoveKeyRequest& in_request) override;
  void MigrateKey(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                      user_data_auth::MigrateKeyReply>> response,
                  const user_data_auth::MigrateKeyRequest& in_request) override;
  void MigrateToDircrypto(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          user_data_auth::MigrateToDircryptoReply>> response,
      const user_data_auth::MigrateToDircryptoRequest& in_request) override;
  void NeedsDircryptoMigration(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          user_data_auth::NeedsDircryptoMigrationReply>> response,
      const user_data_auth::NeedsDircryptoMigrationRequest& in_request)
      override;
  void GetSupportedKeyPolicies(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          user_data_auth::GetSupportedKeyPoliciesReply>> response,
      const user_data_auth::GetSupportedKeyPoliciesRequest& in_request)
      override;
  void GetDiskFeatures(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          user_data_auth::GetDiskFeaturesReply>> response,
      const user_data_auth::GetDiskFeaturesRequest& in_request) override;
  void GetAccountDiskUsage(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          user_data_auth::GetAccountDiskUsageReply>> response,
      const user_data_auth::GetAccountDiskUsageRequest& in_request) override;
  void GetCurrentSpaceForUid(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          user_data_auth::GetCurrentSpaceForUidReply>> response,
      const user_data_auth::GetCurrentSpaceForUidRequest& in_request) override;
  void GetCurrentSpaceForGid(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          user_data_auth::GetCurrentSpaceForGidReply>> response,
      const user_data_auth::GetCurrentSpaceForGidRequest& in_request) override;

 private:
  brillo::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(UserDataAuthAdaptor);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_SERVICE_USERDATAAUTH_H_
