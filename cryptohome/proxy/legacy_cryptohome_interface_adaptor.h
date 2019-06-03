// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_PROXY_LEGACY_CRYPTOHOME_INTERFACE_ADAPTOR_H_
#define CRYPTOHOME_PROXY_LEGACY_CRYPTOHOME_INTERFACE_ADAPTOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <attestation/proto_bindings/interface.pb.h>
#include <attestation-client/attestation/dbus-proxies.h>
#include <base/atomic_sequence_num.h>
#include <base/location.h>
#include <base/optional.h>
#include <dbus/cryptohome/dbus-constants.h>
#include <tpm_manager/proto_bindings/tpm_manager.pb.h>
#include <tpm_manager-client/tpm_manager/dbus-proxies.h>

#include "rpc.pb.h"  // NOLINT(build/include)
#include "UserDataAuth.pb.h"  // NOLINT(build/include)
#include "dbus_adaptors/org.chromium.CryptohomeInterface.h"  // NOLINT(build/include_alpha)
#include "user_data_auth/dbus-proxies.h"
// The dbus_adaptor and proxy include must happen after the protobuf include

namespace cryptohome {

// This class is for holding a DBusMethodResponse in a std::shared_ptr, so that
// we can bind it to two separate callback.
template <typename... Types>
class SharedDBusMethodResponse {
 public:
  explicit SharedDBusMethodResponse(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<Types...>>
          response)
      : response_(std::move(response)) {}

  void ReplyWithError(const brillo::Error* error) {
    CHECK(response_) << "ReplyWithError() called after response has been sent";
    response_->ReplyWithError(error);
    response_.reset(nullptr);
  }

  void ReplyWithError(const base::Location& location,
                      const std::string& error_domain,
                      const std::string& error_code,
                      const std::string& error_message) {
    CHECK(response_) << "ReplyWithError() (4 parameter version) called after "
                        "response has been sent";
    response_->ReplyWithError(location, error_domain, error_code,
                              error_message);
    response_.reset(nullptr);
  }

  void Return(const Types&... return_values) {
    CHECK(response_) << "Return() called after response has been sent";
    response_->Return(return_values...);
    response_.reset(nullptr);
  }

 private:
  std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<Types...>> response_;
};

class LegacyCryptohomeInterfaceAdaptor
    : public org::chromium::CryptohomeInterfaceInterface,
      public org::chromium::CryptohomeInterfaceAdaptor {
 public:
  explicit LegacyCryptohomeInterfaceAdaptor(scoped_refptr<dbus::Bus> bus)
      : org::chromium::CryptohomeInterfaceAdaptor(this),
        dbus_object_(nullptr, bus, dbus::ObjectPath(kCryptohomeServicePath)),
        default_attestation_proxy_(new org::chromium::AttestationProxy(bus)),
        attestation_proxy_(default_attestation_proxy_.get()),
        default_tpm_ownership_proxy_(new org::chromium::TpmOwnershipProxy(bus)),
        tpm_ownership_proxy_(default_tpm_ownership_proxy_.get()),
        default_tpm_nvram_proxy_(new org::chromium::TpmNvramProxy(bus)),
        tpm_nvram_proxy_(default_tpm_nvram_proxy_.get()),
        default_userdataauth_proxy_(
            new org::chromium::UserDataAuthInterfaceProxy(bus)),
        userdataauth_proxy_(default_userdataauth_proxy_.get()),
        default_arc_quota_proxy_(new org::chromium::ArcQuotaProxy(bus)),
        arc_quota_proxy_(default_arc_quota_proxy_.get()),
        default_pkcs11_proxy_(
            new org::chromium::CryptohomePkcs11InterfaceProxy(bus)),
        pkcs11_proxy_(default_pkcs11_proxy_.get()),
        default_install_attributes_proxy_(
            new org::chromium::InstallAttributesInterfaceProxy(bus)),
        install_attributes_proxy_(default_install_attributes_proxy_.get()),
        default_misc_proxy_(
            new org::chromium::CryptohomeMiscInterfaceProxy(bus)),
        misc_proxy_(default_misc_proxy_.get()) {}

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
  void GetRsuDeviceId(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          cryptohome::BaseReply>> response,
      const cryptohome::GetRsuDeviceIdRequest& in_request) override;

 protected:
  // This constructor is reserved only for testing.
  LegacyCryptohomeInterfaceAdaptor(
      org::chromium::AttestationProxyInterface* attestation_proxy,
      org::chromium::TpmOwnershipProxyInterface* tpm_ownership_proxy,
      org::chromium::TpmNvramProxyInterface* tpm_nvram_proxy,
      org::chromium::UserDataAuthInterfaceProxyInterface* userdataauth_proxy,
      org::chromium::ArcQuotaProxyInterface* arc_quota_proxy,
      org::chromium::CryptohomePkcs11InterfaceProxyInterface* pkcs11_proxy,
      org::chromium::InstallAttributesInterfaceProxyInterface*
          install_attributes_proxy,
      org::chromium::CryptohomeMiscInterfaceProxyInterface* misc_proxy)
      : org::chromium::CryptohomeInterfaceAdaptor(this),
        dbus_object_(
            nullptr, nullptr, dbus::ObjectPath(kCryptohomeServicePath)),
        attestation_proxy_(attestation_proxy),
        tpm_ownership_proxy_(tpm_ownership_proxy),
        tpm_nvram_proxy_(tpm_nvram_proxy),
        userdataauth_proxy_(userdataauth_proxy),
        arc_quota_proxy_(arc_quota_proxy),
        pkcs11_proxy_(pkcs11_proxy),
        install_attributes_proxy_(install_attributes_proxy),
        misc_proxy_(misc_proxy) {}

  // This is used in testing to be able to mock SendAsyncCallStatusSignal.
  virtual void VirtualSendAsyncCallStatusSignal(int32_t in_async_id,
                                                bool in_return_status,
                                                int32_t in_return_code) {
    SendAsyncCallStatusSignal(in_async_id, in_return_status, in_return_code);
  }

  // This is used in testing to be able to mock
  // SendAsyncCallStatusWithDataSignal.
  virtual void VirtualSendAsyncCallStatusWithDataSignal(
      int32_t in_async_id,
      bool in_return_status,
      const std::vector<uint8_t>& in_data) {
    SendAsyncCallStatusWithDataSignal(in_async_id, in_return_status, in_data);
  }

 private:
  // Method used as callbacks once the call to the new interface returns
  // Note that OnSuccess in the method names below refers to a successful DBus
  // call, which may or may not be the same as the action being performed by the
  // underlying API is successful. Some of our APIs opt to reflect failure to
  // perform the action that the the API is supposed to do through protobuf
  // fields (such as using CryptohomeErrorCode).
  void IsMountedOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<bool>> response,
      const user_data_auth::IsMountedReply& reply);
  void IsMountedForUserOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<bool, bool>> response,
      const user_data_auth::IsMountedReply& reply);
  void ListKeysExOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<cryptohome::BaseReply>> response,
      const user_data_auth::ListKeysReply& reply);
  void GetKeyDataOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<cryptohome::BaseReply>> response,
      const user_data_auth::GetKeyDataReply& reply);
  void GetSystemSaltOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<std::vector<uint8_t>>> response,
      const user_data_auth::GetSystemSaltReply& reply);
  void MountExOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<cryptohome::BaseReply>> response,
      const user_data_auth::MountReply& reply);
  void TpmIsReadyOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<bool>> response,
      const tpm_manager::GetTpmStatusReply& reply);
  void TpmIsAttestationPreparedOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<bool>> response_raw,
      const attestation::GetEnrollmentPreparationsReply& reply);
  void TpmAttestationCreateEnrollRequestOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<std::vector<uint8_t>>> response,
      const attestation::CreateEnrollRequestReply& reply);
  void TpmAttestationEnrollSuccess(
      std::shared_ptr<SharedDBusMethodResponse<bool>> response,
      const attestation::FinishEnrollReply& reply);
  void TpmAttestationCreateCertRequestOnSuccess(
      std::shared_ptr<SharedDBusMethodResponse<std::vector<uint8_t>>> response,
      const attestation::CreateCertificateRequestReply& reply);

  // This method forwards the error received from calling the new interface back
  // to the old interface
  template <typename... Types>
  void ForwardError(
      std::shared_ptr<SharedDBusMethodResponse<Types...>> response,
      brillo::Error* err) {
    response->ReplyWithError(err);
  }

  // Returns the next sequence ID for Async methods
  int NextSequence() {
    // AtomicSequenceNumber is zero-based, so increment so that the sequence
    // ids are one-based.
    return sequence_holder_.GetNext() + 1;
  }

  // This serves as the on_failure callback after calling the actual method
  // in attestationd
  template <typename ReplyProtoType>
  void AsyncForwardError(const std::string& (ReplyProtoType::*func)() const,
                         int async_id,
                         brillo::Error* err) {
    // Error is ignored because there is no mechanism to forward the dbus
    // error through signal, and the current implementation in
    // service_distributed class handles the error by sending
    // STATUS_NOT_AVAILABLE instead, so we follow this behaviour.
    ReplyProtoType reply;
    reply.set_status(attestation::AttestationStatus::STATUS_NOT_AVAILABLE);
    AsyncReplyWithData(func, async_id, reply);
  }

  // This serves as the on_success callback after calling the actual method
  // in attestationd
  template <typename ReplyProtoType>
  void AsyncReplyWithData(const std::string& (ReplyProtoType::*func)() const,
                          int async_id,
                          const ReplyProtoType& reply) {
    std::string data_string = (reply.*func)();
    std::vector<uint8_t> data(data_string.begin(), data_string.end());
    bool return_status =
        reply.status() == attestation::AttestationStatus::STATUS_SUCCESS;
    VirtualSendAsyncCallStatusWithDataSignal(async_id, return_status, data);
  }

  // This is a function that handles an async request received on the legacy
  // cryptohome interface. The code that calls this function resides in
  // the actual method handler, and it only needs to assemble the request
  // proto and pass it to this function, and this function will take care
  // of the rest.
  template <typename RequestProtoType, typename ReplyProtoType>
  int HandleAsync(const std::string& (ReplyProtoType::*func)() const,
                  RequestProtoType request,
                  base::OnceCallback<
                      void(const RequestProtoType&,
                           const base::Callback<void(const ReplyProtoType&)>&,
                           const base::Callback<void(brillo::Error*)>&,
                           int)> target_method) {
    int async_id = NextSequence();

    base::Callback<void(const ReplyProtoType&)> on_success = base::Bind(
        &LegacyCryptohomeInterfaceAdaptor::AsyncReplyWithData<ReplyProtoType>,
        base::Unretained(this), func, async_id);
    base::Callback<void(brillo::Error*)> on_failure = base::Bind(
        &LegacyCryptohomeInterfaceAdaptor::AsyncForwardError<ReplyProtoType>,
        base::Unretained(this), func, async_id);
    std::move(target_method)
        .Run(request, on_success, on_failure,
             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);

    return async_id;
  }

  // This method is used when the handler for a successful DBus call to the new
  // API only needs to forward the error code in the new API's proto to a
  // BaseReply type on the legacy API.
  template <typename ReplyProtoType>
  static void ForwardBaseReplyErrorCode(
      std::shared_ptr<SharedDBusMethodResponse<cryptohome::BaseReply>> response,
      const ReplyProtoType& reply) {
    cryptohome::BaseReply base_reply;
    base_reply.set_error(
        static_cast<cryptohome::CryptohomeErrorCode>(reply.error()));
    response->Return(base_reply);
  }

  // A helper function which maps an integer to a valid CertificateProfile.
  static attestation::CertificateProfile IntegerToCertificateProfile(
      int profile_value);

  // A helper function which maps an integer to a valid ACAType.
  static base::Optional<attestation::ACAType> IntegerToACAType(int type);

  brillo::dbus_utils::DBusObject dbus_object_;

  // Below are the DBus proxy objects that are used in this class. The default_*
  // versions are for holding an instance that is only used in actual,
  // non-testing setting. While the non default_* versions are the ones that's
  // actually used by the class. During testing, the non default_* versions will
  // be overridden with mocks for testing.
  // Note that this follows the convention in other cryptohome class such as
  // UserDataAuth class.
  std::unique_ptr<org::chromium::AttestationProxyInterface>
      default_attestation_proxy_;
  org::chromium::AttestationProxyInterface* attestation_proxy_;
  std::unique_ptr<org::chromium::TpmOwnershipProxyInterface>
      default_tpm_ownership_proxy_;
  org::chromium::TpmOwnershipProxyInterface* tpm_ownership_proxy_;
  std::unique_ptr<org::chromium::TpmNvramProxyInterface>
      default_tpm_nvram_proxy_;
  org::chromium::TpmNvramProxyInterface* tpm_nvram_proxy_;
  std::unique_ptr<org::chromium::UserDataAuthInterfaceProxyInterface>
      default_userdataauth_proxy_;
  org::chromium::UserDataAuthInterfaceProxyInterface* userdataauth_proxy_;
  std::unique_ptr<org::chromium::ArcQuotaProxyInterface>
      default_arc_quota_proxy_;
  org::chromium::ArcQuotaProxyInterface* arc_quota_proxy_;
  std::unique_ptr<org::chromium::CryptohomePkcs11InterfaceProxyInterface>
      default_pkcs11_proxy_;
  org::chromium::CryptohomePkcs11InterfaceProxyInterface* pkcs11_proxy_;
  std::unique_ptr<org::chromium::InstallAttributesInterfaceProxyInterface>
      default_install_attributes_proxy_;
  org::chromium::InstallAttributesInterfaceProxyInterface*
      install_attributes_proxy_;
  std::unique_ptr<org::chromium::CryptohomeMiscInterfaceProxyInterface>
      default_misc_proxy_;
  org::chromium::CryptohomeMiscInterfaceProxyInterface* misc_proxy_;

  // An atomic incrementing sequence for setting asynchronous call ids.
  base::AtomicSequenceNumber sequence_holder_;

  DISALLOW_COPY_AND_ASSIGN(LegacyCryptohomeInterfaceAdaptor);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PROXY_LEGACY_CRYPTOHOME_INTERFACE_ADAPTOR_H_
