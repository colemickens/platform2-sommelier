// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/service_userdataauth.h"

namespace cryptohome {

void UserDataAuthAdaptor::IsMounted(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::IsMountedReply>> response,
    const user_data_auth::IsMountedRequest& in_request) {
  user_data_auth::IsMountedReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::Unmount(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::UnmountReply>> response,
    const user_data_auth::UnmountRequest& in_request) {
  user_data_auth::UnmountReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::Mount(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::MountReply>> response,
    const user_data_auth::MountRequest& in_request) {
  user_data_auth::MountReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::Remove(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::RemoveReply>> response,
    const user_data_auth::RemoveRequest& in_request) {
  user_data_auth::RemoveReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::Rename(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::RenameReply>> response,
    const user_data_auth::RenameRequest& in_request) {
  user_data_auth::RenameReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::ListKeys(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::ListKeysReply>> response,
    const user_data_auth::ListKeysRequest& in_request) {
  user_data_auth::ListKeysReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::GetKeyData(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetKeyDataReply>> response,
    const user_data_auth::GetKeyDataRequest& in_request) {
  user_data_auth::GetKeyDataReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::CheckKey(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::CheckKeyReply>> response,
    const user_data_auth::CheckKeyRequest& in_request) {
  user_data_auth::CheckKeyReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::AddKey(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::AddKeyReply>> response,
    const user_data_auth::AddKeyRequest& in_request) {
  user_data_auth::AddKeyReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::UpdateKey(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::UpdateKeyReply>> response,
    const user_data_auth::UpdateKeyRequest& in_request) {
  user_data_auth::UpdateKeyReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::RemoveKey(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::RemoveKeyReply>> response,
    const user_data_auth::RemoveKeyRequest& in_request) {
  user_data_auth::RemoveKeyReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::MigrateKey(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::MigrateKeyReply>> response,
    const user_data_auth::MigrateKeyRequest& in_request) {
  user_data_auth::MigrateKeyReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::MigrateToDircrypto(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::MigrateToDircryptoReply>> response,
    const user_data_auth::MigrateToDircryptoRequest& in_request) {
  user_data_auth::MigrateToDircryptoReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::NeedsDircryptoMigration(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::NeedsDircryptoMigrationReply>> response,
    const user_data_auth::NeedsDircryptoMigrationRequest& in_request) {
  user_data_auth::NeedsDircryptoMigrationReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::GetSupportedKeyPolicies(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetSupportedKeyPoliciesReply>> response,
    const user_data_auth::GetSupportedKeyPoliciesRequest& in_request) {
  user_data_auth::GetSupportedKeyPoliciesReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::GetDiskFeatures(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetDiskFeaturesReply>> response,
    const user_data_auth::GetDiskFeaturesRequest& in_request) {
  user_data_auth::GetDiskFeaturesReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::GetAccountDiskUsage(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetAccountDiskUsageReply>> response,
    const user_data_auth::GetAccountDiskUsageRequest& in_request) {
  user_data_auth::GetAccountDiskUsageReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::GetCurrentSpaceForUid(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetCurrentSpaceForUidReply>> response,
    const user_data_auth::GetCurrentSpaceForUidRequest& in_request) {
  user_data_auth::GetCurrentSpaceForUidReply reply;
  response->Return(reply);
}

void UserDataAuthAdaptor::GetCurrentSpaceForGid(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetCurrentSpaceForGidReply>> response,
    const user_data_auth::GetCurrentSpaceForGidRequest& in_request) {
  user_data_auth::GetCurrentSpaceForGidReply reply;
  response->Return(reply);
}

}  // namespace cryptohome
