// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_PROXY_LEGACY_CRYPTOHOME_INTERFACE_ADAPTOR_H_
#define CRYPTOHOME_PROXY_LEGACY_CRYPTOHOME_INTERFACE_ADAPTOR_H_

#include <memory>
#include <string>
#include <vector>

#include <dbus/cryptohome/dbus-constants.h>

#include "rpc.pb.h"  // NOLINT(build/include)
#include "dbus_adaptors/org.chromium.CryptohomeInterface.h"  // NOLINT(build/include_alpha)
// The dbus_adaptors include must happen after the protobuf include

namespace cryptohome {

class LegacyCryptohomeInterfaceAdaptor
    : public org::chromium::CryptohomeInterfaceInterface,
      public org::chromium::CryptohomeInterfaceAdaptor {
 public:
  explicit LegacyCryptohomeInterfaceAdaptor(scoped_refptr<dbus::Bus> bus)
      : org::chromium::CryptohomeInterfaceAdaptor(this),
        dbus_object_(nullptr, bus, dbus::ObjectPath(kCryptohomeServicePath)) {}

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

  // The actual dbus methods
  void IsMounted(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>>
                     response) override;
  void IsMountedForUser(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool, bool>>
          response,
      const std::string& in_username) override;
  void ListKeysEx(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_account_id,
      const cryptohome::AuthorizationRequest& in_authorization_request,
      const cryptohome::ListKeysRequest& in_list_keys_request) override;
  void CheckKeyEx(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_account_id,
      const cryptohome::AuthorizationRequest& in_authorization_request,
      const cryptohome::CheckKeyRequest& in_check_key_request) override;
  void RemoveKeyEx(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_account_id,
      const cryptohome::AuthorizationRequest& in_authorization_request,
      const cryptohome::RemoveKeyRequest& in_remove_key_request) override;
  void GetKeyDataEx(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_account_id,
      const cryptohome::AuthorizationRequest& in_authorization_request,
      const cryptohome::GetKeyDataRequest& in_get_key_data_request) override;
  void MigrateKeyEx(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_account,
      const cryptohome::AuthorizationRequest& in_authorization_request,
      const cryptohome::MigrateKeyRequest& in_migrate_request) override;
  void AddKeyEx(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_account_id,
      const cryptohome::AuthorizationRequest& in_authorization_request,
      const cryptohome::AddKeyRequest& in_add_key_request) override;
  void UpdateKeyEx(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_account_id,
      const cryptohome::AuthorizationRequest& in_authorization_request,
      const cryptohome::UpdateKeyRequest& in_update_key_request) override;
  void RemoveEx(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                    cryptohome::BaseReply>> response,
                const cryptohome::AccountIdentifier& in_account) override;
  void GetSystemSalt(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                         std::vector<uint8_t>>> response) override;
  void GetSanitizedUsername(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::string>>
          response,
      const std::string& in_username) override;
  void MountEx(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                   cryptohome::BaseReply>> response,
               const cryptohome::AccountIdentifier& in_account_id,
               const cryptohome::AuthorizationRequest& in_authorization_request,
               const cryptohome::MountRequest& in_mount_request) override;
  void MountGuestEx(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                        cryptohome::BaseReply>> response,
                    const cryptohome::MountGuestRequest& in_request) override;
  void RenameCryptohome(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_cryptohome_id_from,
      const cryptohome::AccountIdentifier& in_cryptohome_id_to) override;
  void GetAccountDiskUsage(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AccountIdentifier& in_account_id) override;
  void UnmountEx(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                     cryptohome::BaseReply>> response,
                 const cryptohome::UnmountRequest& in_request) override;
  void UpdateCurrentUserActivityTimestamp(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      int32_t in_time_shift_sec) override;
  void TpmIsReady(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>>
                      response) override;
  void TpmIsEnabled(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void TpmGetPassword(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::string>>
          response) override;
  void TpmIsOwned(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>>
                      response) override;
  void TpmIsBeingOwned(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void TpmCanAttemptOwnership(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response)
      override;
  void TpmClearStoredPassword(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response)
      override;
  void TpmIsAttestationPrepared(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void TpmAttestationGetEnrollmentPreparationsEx(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::AttestationGetEnrollmentPreparationsRequest& in_request)
      override;
  void TpmVerifyAttestationData(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
      bool in_is_cros_core) override;
  void TpmVerifyEK(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
      bool in_is_cros_core) override;
  void TpmAttestationCreateEnrollRequest(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          std::vector<uint8_t>>> response,
      int32_t in_pca_type) override;
  void AsyncTpmAttestationCreateEnrollRequest(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
      int32_t in_pca_type) override;
  void TpmAttestationEnroll(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
      int32_t in_pca_type,
      const std::vector<uint8_t>& in_pca_response) override;
  void AsyncTpmAttestationEnroll(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
      int32_t in_pca_type,
      const std::vector<uint8_t>& in_pca_response) override;
  void TpmAttestationCreateCertRequest(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          std::vector<uint8_t>>> response,
      int32_t in_pca_type,
      int32_t in_certificate_profile,
      const std::string& in_username,
      const std::string& in_request_origin) override;
  void AsyncTpmAttestationCreateCertRequest(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
      int32_t in_pca_type,
      int32_t in_certificate_profile,
      const std::string& in_username,
      const std::string& in_request_origin) override;
  void TpmAttestationFinishCertRequest(
      std::unique_ptr<
          brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>, bool>>
          response,
      const std::vector<uint8_t>& in_pca_response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name) override;
  void AsyncTpmAttestationFinishCertRequest(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
      const std::vector<uint8_t>& in_pca_response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name) override;
  void TpmIsAttestationEnrolled(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void TpmAttestationDoesKeyExist(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name) override;
  void TpmAttestationGetCertificate(
      std::unique_ptr<
          brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>, bool>>
          response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name) override;
  void TpmAttestationGetPublicKey(
      std::unique_ptr<
          brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>, bool>>
          response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name) override;
  void TpmAttestationGetEnrollmentId(
      std::unique_ptr<
          brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>, bool>>
          response,
      bool in_ignore_cache) override;
  void TpmAttestationRegisterKey(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name) override;
  void TpmAttestationSignEnterpriseChallenge(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name,
      const std::string& in_domain,
      const std::vector<uint8_t>& in_device_id,
      bool in_include_signed_public_key,
      const std::vector<uint8_t>& in_challenge) override;
  void TpmAttestationSignEnterpriseVaChallenge(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
      int32_t in_va_type,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name,
      const std::string& in_domain,
      const std::vector<uint8_t>& in_device_id,
      bool in_include_signed_public_key,
      const std::vector<uint8_t>& in_challenge) override;
  void TpmAttestationSignSimpleChallenge(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name,
      const std::vector<uint8_t>& in_challenge) override;
  void TpmAttestationGetKeyPayload(
      std::unique_ptr<
          brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>, bool>>
          response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name) override;
  void TpmAttestationSetKeyPayload(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_name,
      const std::vector<uint8_t>& in_payload) override;
  void TpmAttestationDeleteKeys(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
      bool in_is_user_specific,
      const std::string& in_username,
      const std::string& in_key_prefix) override;
  void TpmAttestationGetEK(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::string, bool>>
          response) override;
  void TpmAttestationResetIdentity(
      std::unique_ptr<
          brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>, bool>>
          response,
      const std::string& in_reset_token) override;
  void TpmGetVersionStructured(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<uint32_t,
                                                             uint64_t,
                                                             uint32_t,
                                                             uint32_t,
                                                             uint64_t,
                                                             std::string>>
          response) override;
  void Pkcs11IsTpmTokenReady(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void Pkcs11GetTpmTokenInfo(
      std::unique_ptr<brillo::dbus_utils::
                          DBusMethodResponse<std::string, std::string, int32_t>>
          response) override;
  void Pkcs11GetTpmTokenInfoForUser(
      std::unique_ptr<brillo::dbus_utils::
                          DBusMethodResponse<std::string, std::string, int32_t>>
          response,
      const std::string& in_username) override;
  void Pkcs11Terminate(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      const std::string& in_username) override;
  void GetStatusString(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::string>>
          response) override;
  void InstallAttributesGet(
      std::unique_ptr<
          brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>, bool>>
          response,
      const std::string& in_name) override;
  void InstallAttributesSet(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
      const std::string& in_name,
      const std::vector<uint8_t>& in_value) override;
  void InstallAttributesCount(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response)
      override;
  void InstallAttributesFinalize(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void InstallAttributesIsReady(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void InstallAttributesIsSecure(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void InstallAttributesIsInvalid(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void InstallAttributesIsFirstInstall(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void SignBootLockbox(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::SignBootLockboxRequest& in_request) override;
  void VerifyBootLockbox(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::VerifyBootLockboxRequest& in_request) override;
  void FinalizeBootLockbox(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::FinalizeBootLockboxRequest& in_request) override;
  void GetBootAttribute(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::GetBootAttributeRequest& in_request) override;
  void SetBootAttribute(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::SetBootAttributeRequest& in_request) override;
  void FlushAndSignBootAttributes(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::FlushAndSignBootAttributesRequest& in_request) override;
  void GetLoginStatus(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::GetLoginStatusRequest& in_request) override;
  void GetTpmStatus(std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
                        cryptohome::BaseReply>> response,
                    const cryptohome::GetTpmStatusRequest& in_request) override;
  void GetEndorsementInfo(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::GetEndorsementInfoRequest& in_request) override;
  void InitializeCastKey(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::InitializeCastKeyRequest& in_request) override;
  void GetFirmwareManagementParameters(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::GetFirmwareManagementParametersRequest& in_request)
      override;
  void SetFirmwareManagementParameters(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::SetFirmwareManagementParametersRequest& in_request)
      override;
  void RemoveFirmwareManagementParameters(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::RemoveFirmwareManagementParametersRequest& in_request)
      override;
  void MigrateToDircrypto(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      const cryptohome::AccountIdentifier& in_account_id,
      const cryptohome::MigrateToDircryptoRequest& in_migrate_request) override;
  void NeedsDircryptoMigration(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
      const cryptohome::AccountIdentifier& in_account_id) override;
  void GetSupportedKeyPolicies(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::GetSupportedKeyPoliciesRequest& in_request) override;
  void IsQuotaSupported(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response)
      override;
  void GetCurrentSpaceForUid(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int64_t>> response,
      uint32_t in_uid) override;
  void GetCurrentSpaceForGid(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int64_t>> response,
      uint32_t in_gid) override;
  void LockToSingleUserMountUntilReboot(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::LockToSingleUserMountUntilRebootRequest& in_request)
      override;

 private:
  brillo::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(LegacyCryptohomeInterfaceAdaptor);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PROXY_LEGACY_CRYPTOHOME_INTERFACE_ADAPTOR_H_
