// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <chromeos/libhwsec/task_dispatching_framework.h>

#include "cryptohome/service_userdataauth.h"
#include "cryptohome/userdataauth.h"

namespace cryptohome {

using ::hwsec::ThreadSafeDBusMethodResponse;

void UserDataAuthAdaptor::IsMounted(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::IsMountedReply>> response,
    const user_data_auth::IsMountedRequest& in_request) {
  service_->PostTaskToMountThread(
      FROM_HERE,
      base::BindOnce(
          &UserDataAuthAdaptor::DoIsMounted, base::Unretained(this),
          in_request.username(),
          ThreadSafeDBusMethodResponse<user_data_auth::IsMountedReply>::
              MakeThreadSafe(std::move(response))));
}

void UserDataAuthAdaptor::DoIsMounted(
    const std::string username,
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<user_data_auth::IsMountedReply>>
        response) {
  bool is_ephemeral = false;
  bool is_mounted = service_->IsMounted(username, &is_ephemeral);

  user_data_auth::IsMountedReply reply;
  reply.set_is_mounted(is_mounted);
  reply.set_is_ephemeral_mount(is_ephemeral);
  std::move(response)->Return(reply);
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

void UserDataAuthAdaptor::StartMigrateToDircrypto(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::StartMigrateToDircryptoReply>> response,
    const user_data_auth::StartMigrateToDircryptoRequest& in_request) {
  user_data_auth::StartMigrateToDircryptoReply reply;
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

void UserDataAuthAdaptor::GetAccountDiskUsage(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetAccountDiskUsageReply>> response,
    const user_data_auth::GetAccountDiskUsageRequest& in_request) {
  user_data_auth::GetAccountDiskUsageReply reply;
  response->Return(reply);
}

void ArcQuotaAdaptor::GetArcDiskFeatures(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetArcDiskFeaturesReply>> response,
    const user_data_auth::GetArcDiskFeaturesRequest& in_request) {
  user_data_auth::GetArcDiskFeaturesReply reply;
  response->Return(reply);
}

void ArcQuotaAdaptor::GetCurrentSpaceForArcUid(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetCurrentSpaceForArcUidReply>> response,
    const user_data_auth::GetCurrentSpaceForArcUidRequest& in_request) {
  user_data_auth::GetCurrentSpaceForArcUidReply reply;
  response->Return(reply);
}

void ArcQuotaAdaptor::GetCurrentSpaceForArcGid(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetCurrentSpaceForArcGidReply>> response,
    const user_data_auth::GetCurrentSpaceForArcGidRequest& in_request) {
  user_data_auth::GetCurrentSpaceForArcGidReply reply;
  response->Return(reply);
}

void Pkcs11Adaptor::Pkcs11IsTpmTokenReady(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::Pkcs11IsTpmTokenReadyReply>> response,
    const user_data_auth::Pkcs11IsTpmTokenReadyRequest& in_request) {
  user_data_auth::Pkcs11IsTpmTokenReadyReply reply;
  response->Return(reply);
}

void Pkcs11Adaptor::Pkcs11GetTpmTokeInfo(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::Pkcs11GetTpmTokeInfoReply>> response,
    const user_data_auth::Pkcs11GetTpmTokeInfoRequest& in_request) {
  user_data_auth::Pkcs11GetTpmTokeInfoReply reply;
  response->Return(reply);
}

void Pkcs11Adaptor::Pkcs11Terminate(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::Pkcs11TerminateReply>> response,
    const user_data_auth::Pkcs11TerminateRequest& in_request) {
  user_data_auth::Pkcs11TerminateReply reply;
  response->Return(reply);
}

void InstallAttributesAdaptor::InstallAttributesGet(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::InstallAttributesGetReply>> response,
    const user_data_auth::InstallAttributesGetRequest& in_request) {
  user_data_auth::InstallAttributesGetReply reply;
  response->Return(reply);
}

void InstallAttributesAdaptor::InstallAttributesSet(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::InstallAttributesSetReply>> response,
    const user_data_auth::InstallAttributesSetRequest& in_request) {
  user_data_auth::InstallAttributesSetReply reply;
  response->Return(reply);
}

void InstallAttributesAdaptor::InstallAttributesFinalize(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::InstallAttributesFinalizeReply>> response,
    const user_data_auth::InstallAttributesFinalizeRequest& in_request) {
  user_data_auth::InstallAttributesFinalizeReply reply;
  response->Return(reply);
}

void InstallAttributesAdaptor::InstallAttributesGetStatus(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::InstallAttributesGetStatusReply>> response,
    const user_data_auth::InstallAttributesGetStatusRequest& in_request) {
  user_data_auth::InstallAttributesGetStatusReply reply;
  response->Return(reply);
}

void InstallAttributesAdaptor::GetFirmwareManagementParameters(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetFirmwareManagementParametersReply>> response,
    const user_data_auth::GetFirmwareManagementParametersRequest& in_request) {
  user_data_auth::GetFirmwareManagementParametersReply reply;
  response->Return(reply);
}

void InstallAttributesAdaptor::RemoveFirmwareManagementParameters(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::RemoveFirmwareManagementParametersReply>> response,
    const user_data_auth::RemoveFirmwareManagementParametersRequest&
        in_request) {
  user_data_auth::RemoveFirmwareManagementParametersReply reply;
  response->Return(reply);
}

void InstallAttributesAdaptor::SetFirmwareManagementParameters(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::SetFirmwareManagementParametersReply>> response,
    const user_data_auth::SetFirmwareManagementParametersRequest& in_request) {
  user_data_auth::SetFirmwareManagementParametersReply reply;
  response->Return(reply);
}

void CryptohomeMiscAdaptor::GetSystemSalt(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetSystemSaltReply>> response,
    const user_data_auth::GetSystemSaltRequest& in_request) {
  user_data_auth::GetSystemSaltReply reply;
  response->Return(reply);
}

void CryptohomeMiscAdaptor::UpdateCurrentUserActivityTimestamp(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::UpdateCurrentUserActivityTimestampReply>> response,
    const user_data_auth::UpdateCurrentUserActivityTimestampRequest&
        in_request) {
  user_data_auth::UpdateCurrentUserActivityTimestampReply reply;
  response->Return(reply);
}

void CryptohomeMiscAdaptor::GetSanitizedUsername(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetSanitizedUsernameReply>> response,
    const user_data_auth::GetSanitizedUsernameRequest& in_request) {
  user_data_auth::GetSanitizedUsernameReply reply;
  response->Return(reply);
}

void CryptohomeMiscAdaptor::GetLoginStatus(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetLoginStatusReply>> response,
    const user_data_auth::GetLoginStatusRequest& in_request) {
  user_data_auth::GetLoginStatusReply reply;
  response->Return(reply);
}

void CryptohomeMiscAdaptor::GetStatusString(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        user_data_auth::GetStatusStringReply>> response,
    const user_data_auth::GetStatusStringRequest& in_request) {
  user_data_auth::GetStatusStringReply reply;
  response->Return(reply);
}

}  // namespace cryptohome
