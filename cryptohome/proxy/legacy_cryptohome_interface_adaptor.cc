// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "cryptohome/proxy/legacy_cryptohome_interface_adaptor.h"

namespace cryptohome {

void LegacyCryptohomeInterfaceAdaptor::IsMounted(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::IsMountedForUser(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool, bool>>
        response,
    const std::string& in_username) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::ListKeysEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account_id,
    const cryptohome::AuthorizationRequest& in_authorization_request,
    const cryptohome::ListKeysRequest& in_list_keys_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::CheckKeyEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account_id,
    const cryptohome::AuthorizationRequest& in_authorization_request,
    const cryptohome::CheckKeyRequest& in_check_key_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::RemoveKeyEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account_id,
    const cryptohome::AuthorizationRequest& in_authorization_request,
    const cryptohome::RemoveKeyRequest& in_remove_key_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetKeyDataEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account_id,
    const cryptohome::AuthorizationRequest& in_authorization_request,
    const cryptohome::GetKeyDataRequest& in_get_key_data_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::MigrateKeyEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account,
    const cryptohome::AuthorizationRequest& in_authorization_request,
    const cryptohome::MigrateKeyRequest& in_migrate_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::AddKeyEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account_id,
    const cryptohome::AuthorizationRequest& in_authorization_request,
    const cryptohome::AddKeyRequest& in_add_key_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::UpdateKeyEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account_id,
    const cryptohome::AuthorizationRequest& in_authorization_request,
    const cryptohome::UpdateKeyRequest& in_update_key_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::RemoveEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetSystemSalt(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>>>
        response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetSanitizedUsername(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::string>>
        response,
    const std::string& in_username) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::MountEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account_id,
    const cryptohome::AuthorizationRequest& in_authorization_request,
    const cryptohome::MountRequest& in_mount_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::MountGuestEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::MountGuestRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::RenameCryptohome(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_cryptohome_id_from,
    const cryptohome::AccountIdentifier& in_cryptohome_id_to) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetAccountDiskUsage(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::AccountIdentifier& in_account_id) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::UnmountEx(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::UnmountRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::UpdateCurrentUserActivityTimestamp(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    int32_t in_time_shift_sec) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmIsReady(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmIsEnabled(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmGetPassword(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::string>>
        response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmIsOwned(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmIsBeingOwned(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmCanAttemptOwnership(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmClearStoredPassword(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmIsAttestationPrepared(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::
    TpmAttestationGetEnrollmentPreparationsEx(
        std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
            cryptohome::BaseReply>> response,
        const cryptohome::AttestationGetEnrollmentPreparationsRequest&
            in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmVerifyAttestationData(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    bool in_is_cros_core) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmVerifyEK(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    bool in_is_cros_core) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationCreateEnrollRequest(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>>> response,
    int32_t in_pca_type) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::AsyncTpmAttestationCreateEnrollRequest(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
    int32_t in_pca_type) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationEnroll(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    int32_t in_pca_type,
    const std::vector<uint8_t>& in_pca_response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::AsyncTpmAttestationEnroll(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
    int32_t in_pca_type,
    const std::vector<uint8_t>& in_pca_response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationCreateCertRequest(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>>> response,
    int32_t in_pca_type,
    int32_t in_certificate_profile,
    const std::string& in_username,
    const std::string& in_request_origin) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::AsyncTpmAttestationCreateCertRequest(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
    int32_t in_pca_type,
    int32_t in_certificate_profile,
    const std::string& in_username,
    const std::string& in_request_origin) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationFinishCertRequest(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>,
                                                           bool>> response,
    const std::vector<uint8_t>& in_pca_response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::AsyncTpmAttestationFinishCertRequest(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
    const std::vector<uint8_t>& in_pca_response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmIsAttestationEnrolled(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationDoesKeyExist(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationGetCertificate(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>,
                                                           bool>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationGetPublicKey(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>,
                                                           bool>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationGetEnrollmentId(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>,
                                                           bool>> response,
    bool in_ignore_cache) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationRegisterKey(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationSignEnterpriseChallenge(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name,
    const std::string& in_domain,
    const std::vector<uint8_t>& in_device_id,
    bool in_include_signed_public_key,
    const std::vector<uint8_t>& in_challenge) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationSignEnterpriseVaChallenge(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
    int32_t in_va_type,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name,
    const std::string& in_domain,
    const std::vector<uint8_t>& in_device_id,
    bool in_include_signed_public_key,
    const std::vector<uint8_t>& in_challenge) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationSignSimpleChallenge(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name,
    const std::vector<uint8_t>& in_challenge) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationGetKeyPayload(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>,
                                                           bool>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationSetKeyPayload(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_name,
    const std::vector<uint8_t>& in_payload) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationDeleteKeys(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    bool in_is_user_specific,
    const std::string& in_username,
    const std::string& in_key_prefix) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationGetEK(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::string, bool>>
        response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmAttestationResetIdentity(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>,
                                                           bool>> response,
    const std::string& in_reset_token) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::TpmGetVersionStructured(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<uint32_t,
                                                           uint64_t,
                                                           uint32_t,
                                                           uint32_t,
                                                           uint64_t,
                                                           std::string>>
        response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::Pkcs11IsTpmTokenReady(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::Pkcs11GetTpmTokenInfo(
    std::unique_ptr<brillo::dbus_utils::
                        DBusMethodResponse<std::string, std::string, int32_t>>
        response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::Pkcs11GetTpmTokenInfoForUser(
    std::unique_ptr<brillo::dbus_utils::
                        DBusMethodResponse<std::string, std::string, int32_t>>
        response,
    const std::string& in_username) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::Pkcs11Terminate(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::string& in_username) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetStatusString(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::string>>
        response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InstallAttributesGet(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>,
                                                           bool>> response,
    const std::string& in_name) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InstallAttributesSet(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    const std::string& in_name,
    const std::vector<uint8_t>& in_value) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InstallAttributesCount(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InstallAttributesFinalize(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InstallAttributesIsReady(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InstallAttributesIsSecure(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InstallAttributesIsInvalid(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InstallAttributesIsFirstInstall(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::SignBootLockbox(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::SignBootLockboxRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::VerifyBootLockbox(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::VerifyBootLockboxRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::FinalizeBootLockbox(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::FinalizeBootLockboxRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetBootAttribute(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::GetBootAttributeRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::SetBootAttribute(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::SetBootAttributeRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::FlushAndSignBootAttributes(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::FlushAndSignBootAttributesRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetLoginStatus(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::GetLoginStatusRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetTpmStatus(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::GetTpmStatusRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetEndorsementInfo(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::GetEndorsementInfoRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::InitializeCastKey(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::InitializeCastKeyRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetFirmwareManagementParameters(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::GetFirmwareManagementParametersRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::SetFirmwareManagementParameters(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::SetFirmwareManagementParametersRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::RemoveFirmwareManagementParameters(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::RemoveFirmwareManagementParametersRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::MigrateToDircrypto(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const cryptohome::AccountIdentifier& in_account_id,
    const cryptohome::MigrateToDircryptoRequest& in_migrate_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::NeedsDircryptoMigration(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    const cryptohome::AccountIdentifier& in_account_id) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetSupportedKeyPolicies(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::GetSupportedKeyPoliciesRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::IsQuotaSupported(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetCurrentSpaceForUid(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int64_t>> response,
    uint32_t in_uid) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::GetCurrentSpaceForGid(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int64_t>> response,
    uint32_t in_gid) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

void LegacyCryptohomeInterfaceAdaptor::LockToSingleUserMountUntilReboot(
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<cryptohome::BaseReply>> response,
    const cryptohome::LockToSingleUserMountUntilRebootRequest& in_request) {
  // Not implemented yet
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           DBUS_ERROR_NOT_SUPPORTED,
                           "Method unimplemented yet");
}

}  // namespace cryptohome
