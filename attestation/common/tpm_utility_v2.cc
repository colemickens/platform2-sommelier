//
// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "attestation/common/tpm_utility_v2.h"

#include <memory>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <crypto/scoped_openssl_types.h>
#include <crypto/sha2.h>
#include <openssl/rsa.h>

#include "tpm_manager/common/tpm_manager_constants.h"
#include "trunks/authorization_delegate.h"
#include "trunks/error_codes.h"
#include "trunks/tpm_generated.h"

namespace {

using trunks::AuthorizationDelegate;
using trunks::HmacSession;
using trunks::TPM_HANDLE;
using trunks::TPM_RC;
using trunks::TPM_RC_SUCCESS;

const unsigned int kWellKnownExponent = 65537;
const uint32_t kRSAEndorsementCertificateIndex = 0xC00000;
const uint32_t kECCEndorsementCertificateIndex = 0xC00001;

crypto::ScopedRSA CreateRSAFromRawModulus(uint8_t* modulus_buffer,
                                          size_t modulus_size) {
  crypto::ScopedRSA rsa(RSA_new());
  if (!rsa.get())
    return crypto::ScopedRSA();
  rsa->e = BN_new();
  if (!rsa->e)
    return crypto::ScopedRSA();
  BN_set_word(rsa->e, kWellKnownExponent);
  rsa->n = BN_bin2bn(modulus_buffer, modulus_size, NULL);
  if (!rsa->n)
    return crypto::ScopedRSA();
  return rsa;
}

// An authorization delegate to manage multiple authorization sessions for a
// single command.
class MultipleAuthorizations : public AuthorizationDelegate {
 public:
  MultipleAuthorizations() = default;
  ~MultipleAuthorizations() override = default;

  void AddAuthorizationDelegate(AuthorizationDelegate* delegate) {
    delegates_.push_back(delegate);
  }

  bool GetCommandAuthorization(const std::string& command_hash,
                               bool is_command_parameter_encryption_possible,
                               bool is_response_parameter_encryption_possible,
                               std::string* authorization) override {
    std::string combined_authorization;
    for (auto delegate : delegates_) {
      std::string authorization;
      if (!delegate->GetCommandAuthorization(
              command_hash, is_command_parameter_encryption_possible,
              is_response_parameter_encryption_possible, &authorization)) {
        return false;
      }
      combined_authorization += authorization;
    }
    *authorization = combined_authorization;
    return true;
  }

  bool CheckResponseAuthorization(const std::string& response_hash,
                                  const std::string& authorization) override {
    std::string mutable_authorization = authorization;
    for (auto delegate : delegates_) {
      if (!delegate->CheckResponseAuthorization(
              response_hash,
              ExtractSingleAuthorizationResponse(&mutable_authorization))) {
        return false;
      }
    }
    return true;
  }

  bool EncryptCommandParameter(std::string* parameter) override {
    for (auto delegate : delegates_) {
      if (!delegate->EncryptCommandParameter(parameter)) {
        return false;
      }
    }
    return true;
  }

  bool DecryptResponseParameter(std::string* parameter) override {
    for (auto delegate : delegates_) {
      if (!delegate->DecryptResponseParameter(parameter)) {
        return false;
      }
    }
    return true;
  }

  bool GetTpmNonce(std::string* nonce) override {
    return false;
  }

 private:
  std::string ExtractSingleAuthorizationResponse(std::string* all_responses) {
    std::string response;
    trunks::TPMS_AUTH_RESPONSE not_used;
    if (TPM_RC_SUCCESS !=
        Parse_TPMS_AUTH_RESPONSE(all_responses, &not_used, &response)) {
      return std::string();
    }
    return response;
  }

  std::vector<AuthorizationDelegate*> delegates_;
};

void FlushObject(trunks::TrunksFactory* factory, TPM_HANDLE object_handle) {
  if (object_handle >= trunks::TRANSIENT_FIRST &&
      object_handle <= trunks::TRANSIENT_LAST) {
    factory->GetTpm()->FlushContextSync(object_handle,
                                        nullptr /* authorization */);
  }
}

class TpmObjectScoper {
 public:
  TpmObjectScoper(trunks::TrunksFactory* factory, TPM_HANDLE object_handle)
      : factory_(factory), object_handle_(object_handle) {}
  ~TpmObjectScoper() { FlushObject(factory_, object_handle_); }

 private:
  trunks::TrunksFactory* factory_;
  TPM_HANDLE object_handle_;
};

}  // namespace

namespace attestation {

TpmUtilityV2::TpmUtilityV2(tpm_manager::TpmOwnershipInterface* tpm_owner,
                           tpm_manager::TpmNvramInterface* tpm_nvram,
                           trunks::TrunksFactory* trunks_factory)
    : tpm_owner_(tpm_owner),
      tpm_nvram_(tpm_nvram),
      trunks_factory_(trunks_factory) {}

TpmUtilityV2::~TpmUtilityV2() {
  for (auto& i : endorsement_keys_) {
    FlushObject(trunks_factory_, i.second);
  }
}

void TpmUtilityV2::InitializationTask(base::WaitableEvent* completion) {
  CHECK(completion);
  CHECK(tpm_manager_thread_.task_runner()->RunsTasksOnCurrentThread());

  default_tpm_owner_ = std::make_unique<tpm_manager::TpmOwnershipDBusProxy>();
  default_tpm_nvram_ = std::make_unique<tpm_manager::TpmNvramDBusProxy>();
  if (default_tpm_owner_->Initialize()) {
    tpm_owner_ = default_tpm_owner_.get();
  }
  if (default_tpm_nvram_->Initialize()) {
    tpm_nvram_ = default_tpm_nvram_.get();
  }
  completion->Signal();
}

void TpmUtilityV2::ShutdownTask() {
  tpm_owner_ = nullptr;
  tpm_nvram_ = nullptr;
  default_tpm_owner_.reset(nullptr);
  default_tpm_nvram_.reset(nullptr);
}

bool TpmUtilityV2::Initialize() {
  if (!tpm_manager_thread_.StartWithOptions(base::Thread::Options(
          base::MessageLoopForIO::TYPE_IO, 0 /* Default stack size. */))) {
    LOG(ERROR) << "Failed to start tpm_manager thread.";
    return false;
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    tpm_manager_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&TpmUtilityV2::InitializationTask,
                              base::Unretained(this), &event));
    event.Wait();
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    LOG(ERROR) << "Failed to initialize tpm_managerd clients.";
    return false;
  }
  if (!trunks_factory_) {
    default_trunks_factory_ = std::make_unique<trunks::TrunksFactoryImpl>();
    if (!default_trunks_factory_->Initialize()) {
      LOG(ERROR) << "Failed to initialize trunks.";
      return false;
    }
    trunks_factory_ = default_trunks_factory_.get();
  }
  trunks_utility_ = trunks_factory_->GetTpmUtility();
  return true;
}

bool TpmUtilityV2::IsTpmReady() {
  if (!is_ready_) {
    CacheTpmState();
  }
  return is_ready_;
}

bool TpmUtilityV2::ActivateIdentity(const std::string& delegate_blob,
                                    const std::string& delegate_secret,
                                    const std::string& identity_key_blob,
                                    const std::string& asym_ca_contents,
                                    const std::string& sym_ca_attestation,
                                    std::string* credential) {
  LOG(ERROR) << __func__ << ": Not implemented.";
  return false;
}

bool TpmUtilityV2::ActivateIdentityForTpm2(
    KeyType key_type,
    const std::string& identity_key_blob,
    const std::string& encrypted_seed,
    const std::string& credential_mac,
    const std::string& wrapped_credential,
    std::string* credential) {
  std::unique_ptr<AuthorizationDelegate> empty_password_authorization =
      trunks_factory_->GetPasswordAuthorization(std::string());
  TPM_HANDLE identity_key_handle;
  TPM_RC result = trunks_utility_->LoadKey(identity_key_blob,
                                           empty_password_authorization.get(),
                                           &identity_key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to load identity key: "
               << trunks::GetErrorString(result);
    return false;
  }
  TpmObjectScoper scoper(trunks_factory_, identity_key_handle);
  std::string identity_key_name;
  result = trunks_utility_->GetKeyName(identity_key_handle, &identity_key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get identity key name: "
               << trunks::GetErrorString(result);
    return false;
  }

  TPM_HANDLE endorsement_key_handle;
  if (!GetEndorsementKey(key_type, &endorsement_key_handle)) {
    LOG(ERROR) << __func__ << ": Endorsement key is not available.";
    return false;
  }
  std::string endorsement_key_name;
  result = trunks_utility_->GetKeyName(endorsement_key_handle,
                                       &endorsement_key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get endorsement key name: "
               << trunks::GetErrorString(result);
    return false;
  }

  std::string endorsement_password;
  if (!GetEndorsementPassword(&endorsement_password)) {
    LOG(ERROR) << __func__ << ": Failed to get endorsement password";
    return false;
  }

  std::unique_ptr<HmacSession> endorsement_session =
      trunks_factory_->GetHmacSession();
  result =
      endorsement_session->StartUnboundSession(true /* salted */,
                                               false /* enable_encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to setup endorsement session: "
               << trunks::GetErrorString(result);
    return false;
  }
  endorsement_session->SetEntityAuthorizationValue(endorsement_password);

  std::unique_ptr<trunks::PolicySession> session =
      trunks_factory_->GetPolicySession();
  result = session->StartUnboundSession(true /* salted */,
                                        false /* enable_encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to start session: "
               << trunks::GetErrorString(result);
    return false;
  }

  trunks::TPMI_DH_ENTITY auth_entity = trunks::TPM_RH_ENDORSEMENT;
  std::string auth_entity_name;
  trunks::Serialize_TPM_HANDLE(auth_entity, &auth_entity_name);

  result = session->PolicySecret(auth_entity, auth_entity_name, std::string(),
                                 std::string(), std::string(), 0,
                                 endorsement_session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to set the secret: "
               << trunks::GetErrorString(result);
    return false;
  }

  MultipleAuthorizations authorization;
  authorization.AddAuthorizationDelegate(empty_password_authorization.get());
  authorization.AddAuthorizationDelegate(session->GetDelegate());
  std::string identity_object_data;
  trunks::Serialize_TPM2B_DIGEST(trunks::Make_TPM2B_DIGEST(credential_mac),
                                 &identity_object_data);
  identity_object_data += wrapped_credential;
  trunks::TPM2B_DIGEST encoded_credential;
  result = trunks_factory_->GetTpm()->ActivateCredentialSync(
      identity_key_handle, identity_key_name, endorsement_key_handle,
      endorsement_key_name, trunks::Make_TPM2B_ID_OBJECT(identity_object_data),
      trunks::Make_TPM2B_ENCRYPTED_SECRET(encrypted_seed), &encoded_credential,
      &authorization);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to activate: " << trunks::GetErrorString(result);
    return false;
  }
  *credential = trunks::StringFrom_TPM2B_DIGEST(encoded_credential);
  return true;
}

bool TpmUtilityV2::CreateCertifiedKey(KeyType key_type,
                                      KeyUsage key_usage,
                                      const std::string& identity_key_blob,
                                      const std::string& external_data,
                                      std::string* key_blob,
                                      std::string* public_key_der,
                                      std::string* public_key_tpm_format,
                                      std::string* key_info,
                                      std::string* proof) {
  if (key_type != KEY_TYPE_RSA) {
    LOG(ERROR) << __func__ << ": Not implemented.";
    return false;
  }
  std::unique_ptr<AuthorizationDelegate> empty_password_authorization =
      trunks_factory_->GetPasswordAuthorization(std::string());
  trunks::TpmUtility::AsymmetricKeyUsage trunks_key_usage =
      (key_usage == KEY_USAGE_SIGN) ? trunks::TpmUtility::kSignKey
                                    : trunks::TpmUtility::kDecryptKey;
  TPM_RC result = trunks_utility_->CreateRSAKeyPair(
      trunks_key_usage, 2048 /* modulus_bits */,
      0 /* Use default public exponent */, std::string() /* password */,
      std::string() /* policy_digest */,
      false /* use_only_policy_authorization */,
      std::vector<uint32_t>() /* creation_pcr_indexes */,
      empty_password_authorization.get(), key_blob,
      nullptr /* creation_blob */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to create key: " << trunks::GetErrorString(result);
    return false;
  }
  TPM_HANDLE key_handle;
  result = trunks_utility_->LoadKey(
      *key_blob, empty_password_authorization.get(), &key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to load key: " << trunks::GetErrorString(result);
    return false;
  }
  TpmObjectScoper scoper(trunks_factory_, key_handle);
  std::string key_name;
  result = trunks_utility_->GetKeyName(key_handle, &key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get key name: "
               << trunks::GetErrorString(result);
    return false;
  }
  trunks::TPMT_PUBLIC public_area;
  result = trunks_utility_->GetKeyPublicArea(key_handle, &public_area);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get key public area: "
               << trunks::GetErrorString(result);
    return false;
  }
  result = trunks::Serialize_TPMT_PUBLIC(public_area, public_key_tpm_format);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to serialize key public area: "
               << trunks::GetErrorString(result);
    return false;
  }
  if (!GetRSAPublicKeyFromTpmPublicKey(*public_key_tpm_format,
                                       public_key_der)) {
    LOG(ERROR) << __func__ << ": Failed to convert public key.";
    return false;
  }
  TPM_HANDLE identity_key_handle;
  result = trunks_utility_->LoadKey(identity_key_blob,
                                    empty_password_authorization.get(),
                                    &identity_key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to load key: " << trunks::GetErrorString(result);
    return false;
  }
  TpmObjectScoper scoper2(trunks_factory_, identity_key_handle);
  std::string identity_key_name;
  result = trunks_utility_->GetKeyName(identity_key_handle, &identity_key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get key name: "
               << trunks::GetErrorString(result);
    return false;
  }
  trunks::TPMT_SIG_SCHEME scheme;
  scheme.scheme = trunks::TPM_ALG_RSASSA;
  scheme.details.rsassa.hash_alg = trunks::TPM_ALG_SHA256;
  trunks::TPM2B_ATTEST certify_info;
  trunks::TPMT_SIGNATURE signature;
  MultipleAuthorizations authorization;
  authorization.AddAuthorizationDelegate(empty_password_authorization.get());
  authorization.AddAuthorizationDelegate(empty_password_authorization.get());
  result = trunks_factory_->GetTpm()->CertifySync(
      key_handle, key_name, identity_key_handle, identity_key_name,
      trunks::Make_TPM2B_DATA(external_data), scheme, &certify_info, &signature,
      &authorization);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to certify key: " << trunks::GetErrorString(result);
    return false;
  }
  *key_info = StringFrom_TPM2B_ATTEST(certify_info);
  *proof = StringFrom_TPM2B_PUBLIC_KEY_RSA(signature.signature.rsassa.sig);
  return true;
}

bool TpmUtilityV2::SealToPCR0(const std::string& data,
                              std::string* sealed_data) {
  std::string policy_digest;
  TPM_RC result = trunks_utility_->GetPolicyDigestForPcrValues(
      std::map<uint32_t, std::string>(
          {{0, std::string() /* Use current PCR value */}}),
      false, /* No authorization session */
      &policy_digest);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to compute policy digest: "
               << trunks::GetErrorString(result);
    return false;
  }
  std::unique_ptr<AuthorizationDelegate> empty_password_authorization =
      trunks_factory_->GetPasswordAuthorization(std::string());
  result = trunks_utility_->SealData(
      data, policy_digest, "", empty_password_authorization.get(), sealed_data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to seal data: " << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TpmUtilityV2::Unseal(const std::string& sealed_data, std::string* data) {
  std::unique_ptr<trunks::PolicySession> session =
      trunks_factory_->GetPolicySession();
  TPM_RC result = session->StartUnboundSession(true /* salted */,
                                               true /* enable_encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to start encrypted session: "
               << trunks::GetErrorString(result);
    return false;
  }
  result = session->PolicyPCR(std::map<uint32_t, std::string>(
      {{0, std::string() /* Use current PCR value */}}));
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to setup policy session: "
               << trunks::GetErrorString(result);
    return false;
  }
  result =
      trunks_utility_->UnsealData(sealed_data, session->GetDelegate(), data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to unseal data: " << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TpmUtilityV2::GetEndorsementPublicKey(KeyType key_type,
                                           std::string* public_key_der) {
  TPM_HANDLE key_handle;
  if (!GetEndorsementKey(key_type, &key_handle)) {
    LOG(ERROR) << __func__ << ": EK not available.";
    return false;
  }
  trunks::TPMT_PUBLIC public_area;
  TPM_RC result = trunks_utility_->GetKeyPublicArea(key_handle, &public_area);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get EK public key: "
               << trunks::GetErrorString(result);
    return false;
  }
  std::string public_key_tpm_format;
  result = trunks::Serialize_TPMT_PUBLIC(public_area, &public_key_tpm_format);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to serialize EK public key: "
               << trunks::GetErrorString(result);
    return false;
  }
  if (!GetRSAPublicKeyFromTpmPublicKey(public_key_tpm_format, public_key_der)) {
    LOG(ERROR) << __func__ << ": Failed to convert EK public key.";
    return false;
  }
  return true;
}

bool TpmUtilityV2::GetEndorsementCertificate(KeyType key_type,
                                             std::string* certificate) {
  uint32_t index = (key_type == KEY_TYPE_RSA) ? kRSAEndorsementCertificateIndex
                                              : kECCEndorsementCertificateIndex;
  tpm_manager::ReadSpaceRequest request;
  request.set_index(index);
  tpm_manager::ReadSpaceReply response;
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmNvramInterface::ReadSpace,
                 base::Unretained(tpm_nvram_), request),
      &response);
  if (response.result() == tpm_manager::NVRAM_RESULT_SPACE_DOES_NOT_EXIST) {
    LOG(ERROR) << __func__ << ": Endorsement certificate does not exist.";
    return false;
  }
  if (response.result() != tpm_manager::NVRAM_RESULT_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to read endorsement certificate: "
               << response.result();
    return false;
  }
  *certificate = response.data();
  return true;
}

bool TpmUtilityV2::Unbind(const std::string& key_blob,
                          const std::string& bound_data,
                          std::string* data) {
  std::unique_ptr<AuthorizationDelegate> empty_password_authorization =
      trunks_factory_->GetPasswordAuthorization(std::string());
  TPM_HANDLE key_handle;
  TPM_RC result = trunks_utility_->LoadKey(
      key_blob, empty_password_authorization.get(), &key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to load key: " << trunks::GetErrorString(result);
    return false;
  }
  TpmObjectScoper scoper(trunks_factory_, key_handle);
  result = trunks_utility_->AsymmetricDecrypt(
      key_handle, trunks::TPM_ALG_OAEP, trunks::TPM_ALG_SHA256, bound_data,
      empty_password_authorization.get(), data);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to decrypt: " << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TpmUtilityV2::Sign(const std::string& key_blob,
                        const std::string& data_to_sign,
                        std::string* signature) {
  std::unique_ptr<AuthorizationDelegate> empty_password_authorization =
      trunks_factory_->GetPasswordAuthorization(std::string());
  TPM_HANDLE key_handle;
  TPM_RC result = trunks_utility_->LoadKey(
      key_blob, empty_password_authorization.get(), &key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to load key: " << trunks::GetErrorString(result);
    return false;
  }
  TpmObjectScoper scoper(trunks_factory_, key_handle);
  result = trunks_utility_->Sign(key_handle, trunks::TPM_ALG_RSASSA,
                                 trunks::TPM_ALG_SHA256, data_to_sign,
                                 true /* generate_hash */,
                                 empty_password_authorization.get(), signature);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to sign data: " << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TpmUtilityV2::CreateRestrictedKey(KeyType key_type,
                                       KeyUsage key_usage,
                                       std::string* public_key_der,
                                       std::string* public_key_tpm_format,
                                       std::string* private_key_blob) {
  if (key_usage != KEY_USAGE_SIGN) {
    LOG(ERROR) << __func__ << ": Not implemented.";
    return false;
  }
  std::unique_ptr<AuthorizationDelegate> empty_password_authorization =
      trunks_factory_->GetPasswordAuthorization(std::string());
  trunks::TPM_ALG_ID algorithm =
      (key_type == KEY_TYPE_RSA) ? trunks::TPM_ALG_RSA : trunks::TPM_ALG_ECC;
  TPM_RC result = trunks_utility_->CreateIdentityKey(
      algorithm, empty_password_authorization.get(), private_key_blob);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to create restricted key: "
               << trunks::GetErrorString(result);
    return false;
  }
  std::unique_ptr<trunks::BlobParser> parser = trunks_factory_->GetBlobParser();
  trunks::TPM2B_PUBLIC public_info;
  trunks::TPM2B_PRIVATE not_used;
  if (!parser->ParseKeyBlob(*private_key_blob, &public_info, &not_used)) {
    LOG(ERROR) << __func__ << ": Failed to parse key blob.";
    return false;
  }
  result = trunks::Serialize_TPMT_PUBLIC(public_info.public_area,
                                         public_key_tpm_format);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to serialize public key: "
               << trunks::GetErrorString(result);
    return false;
  }
  if (!GetRSAPublicKeyFromTpmPublicKey(*public_key_tpm_format,
                                       public_key_der)) {
    LOG(ERROR) << __func__ << ": Failed to convert public key to DER encoded";
    return false;
  }
  return true;
}

bool TpmUtilityV2::QuotePCR(uint32_t pcr_index,
                            const std::string& key_blob,
                            std::string* quoted_pcr_value,
                            std::string* quoted_data,
                            std::string* quote) {
  TPM_RC result = trunks_utility_->ReadPCR(pcr_index, quoted_pcr_value);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to read PCR " << pcr_index << ": "
               << trunks::GetErrorString(result);
    return false;
  }
  std::unique_ptr<AuthorizationDelegate> empty_password_authorization =
      trunks_factory_->GetPasswordAuthorization(std::string());
  TPM_HANDLE key_handle;
  result = trunks_utility_->LoadKey(
      key_blob, empty_password_authorization.get(), &key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to load key: " << trunks::GetErrorString(result);
    return false;
  }
  TpmObjectScoper scoper(trunks_factory_, key_handle);
  std::string key_name;
  result = trunks_utility_->GetKeyName(key_handle, &key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get key name: "
               << trunks::GetErrorString(result);
    return false;
  }
  trunks::TPMT_SIG_SCHEME scheme;
  scheme.scheme = trunks::TPM_ALG_RSASSA;
  scheme.details.rsassa.hash_alg = trunks::TPM_ALG_SHA256;
  // This process of selecting pcrs is highlighted in TPM 2.0 Library Spec
  // Part 2 (Section 10.5 - PCR structures).
  trunks::TPML_PCR_SELECTION pcr_selection;
  uint8_t pcr_select_index = pcr_index / 8;
  uint8_t pcr_select_byte = 1 << (pcr_index % 8);
  pcr_selection.count = 1;
  pcr_selection.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_selection.pcr_selections[0].sizeof_select = PCR_SELECT_MIN;
  memset(pcr_selection.pcr_selections[0].pcr_select, 0, PCR_SELECT_MIN);
  pcr_selection.pcr_selections[0].pcr_select[pcr_select_index] =
      pcr_select_byte;
  trunks::TPM2B_ATTEST quoted_struct;
  trunks::TPMT_SIGNATURE signature;
  result = trunks_factory_->GetTpm()->QuoteSync(
      key_handle, key_name,
      trunks::Make_TPM2B_DATA("") /* No qualifying data */, scheme,
      pcr_selection, &quoted_struct, &signature,
      empty_password_authorization.get());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to quote PCR " << pcr_index << ": "
               << trunks::GetErrorString(result);
    return false;
  }
  *quoted_data = StringFrom_TPM2B_ATTEST(quoted_struct);
  *quote = StringFrom_TPM2B_PUBLIC_KEY_RSA(signature.signature.rsassa.sig);
  return true;
}

bool TpmUtilityV2::IsQuoteForPCR(const std::string& quote,
                                 uint32_t pcr_index) const {
  std::string buffer = quote;
  trunks::TPMS_ATTEST parsed_quote;
  TPM_RC result = trunks::Parse_TPMS_ATTEST(&buffer, &parsed_quote, nullptr);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to parse the quote: "
               << trunks::GetErrorString(result);
    return false;
  }
  if (parsed_quote.magic != trunks::TPM_GENERATED_VALUE) {
    LOG(ERROR) << __func__ << ": Bad magic value";
    return false;
  }
  if (parsed_quote.type != trunks::TPM_ST_ATTEST_QUOTE) {
    LOG(ERROR) << __func__ << ": Not a quote";
    return false;
  }
  trunks::TPML_PCR_SELECTION* pcr_select =
      &parsed_quote.attested.quote.pcr_select;
  if (pcr_select->count != 1) {
    LOG(ERROR) << __func__ << ": PCR selection count=" << pcr_select->count;
    return false;
  }
  uint32_t pcr_select_byte = pcr_index / 8;
  trunks::BYTE pcr_select_mask = 1 << (pcr_index % 8);
  trunks::TPMS_PCR_SELECTION* pcr_selection = pcr_select->pcr_selections;
  if (pcr_selection->sizeof_select <= pcr_select_byte) {
    LOG(ERROR) << __func__ << ": PCR selection is too short: "
               << pcr_selection->sizeof_select;
    return false;
  }
  int i;
  for (i = 0; i < pcr_selection->sizeof_select; ++i) {
    if (i == pcr_select_byte) {
      if (pcr_selection->pcr_select[i] != pcr_select_mask) {
        LOG(ERROR) << __func__ << ": wrong bits in PCR selection mask at " << i;
        return false;
      }
    } else {
      if (pcr_selection->pcr_select[i]) {
        LOG(ERROR) << __func__ << ": non-zero byte in PCR selection mask at "
                   << i;
        return false;
      }
    }
  }
  return true;
}

bool TpmUtilityV2::ReadPCR(uint32_t pcr_index, std::string* pcr_value) const {
  TPM_RC result = trunks_utility_->ReadPCR(pcr_index, pcr_value);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to read PCR " << pcr_index << ": "
               << trunks::GetErrorString(result);
    return false;
  }
  return true;
}

bool TpmUtilityV2::CertifyNV(uint32_t nv_index,
                             int nv_size,
                             const std::string& key_blob,
                             std::string* quoted_data,
                             std::string* quote) {
  TPM_RC result;

  std::unique_ptr<AuthorizationDelegate> empty_password_authorization =
      trunks_factory_->GetPasswordAuthorization(std::string());

  MultipleAuthorizations authorization;
  authorization.AddAuthorizationDelegate(empty_password_authorization.get());
  authorization.AddAuthorizationDelegate(empty_password_authorization.get());

  TPM_HANDLE key_handle;
  result = trunks_utility_->LoadKey(
            key_blob, empty_password_authorization.get(),
            &key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to load key: " << trunks::GetErrorString(result);
    return false;
  }
  TpmObjectScoper scoper(trunks_factory_, key_handle);
  std::string key_name;
  result = trunks_utility_->GetKeyName(key_handle, &key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get key name: "
               << trunks::GetErrorString(result);
    return false;
  }

  trunks::TPMT_SIG_SCHEME scheme;
  scheme.scheme = trunks::TPM_ALG_RSASSA;
  scheme.details.rsassa.hash_alg = trunks::TPM_ALG_SHA256;

  trunks::TPM2B_ATTEST quoted_struct;
  trunks::TPMT_SIGNATURE signature;
  result = trunks_factory_->GetTpm()->NV_CertifySync(
      key_handle,  // sign_handle
      key_name,    // sign_handle_name
      nv_index,    // auth_handle
      "",          // auth_handle_name
      nv_index,    // nv_index
      "",          // nv_index_name
      trunks::Make_TPM2B_DATA(""),  // qualifying data
      scheme,      // in_scheme
      nv_size,     // size to read
      0,           // offset
      &quoted_struct,
      &signature,
      &authorization);

  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to certify the NVs: "
               << trunks::GetErrorString(result);
    return false;
  }

  *quoted_data = StringFrom_TPM2B_ATTEST(quoted_struct);
  *quote = StringFrom_TPM2B_PUBLIC_KEY_RSA(signature.signature.rsassa.sig);
  return true;
}


bool TpmUtilityV2::GetRSAPublicKeyFromTpmPublicKey(
    const std::string& tpm_public_key_object,
    std::string* public_key_der) {
  std::string buffer = tpm_public_key_object;
  trunks::TPMT_PUBLIC parsed_public_object;
  TPM_RC result =
      trunks::Parse_TPMT_PUBLIC(&buffer, &parsed_public_object, nullptr);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to parse public key: "
               << trunks::GetErrorString(result);
    return false;
  }
  crypto::ScopedRSA rsa =
      CreateRSAFromRawModulus(parsed_public_object.unique.rsa.buffer,
                              parsed_public_object.unique.rsa.size);
  if (!rsa.get()) {
    LOG(ERROR) << __func__ << ": Failed to decode public key.";
    return false;
  }
  unsigned char* openssl_buffer = NULL;
  int length = i2d_RSAPublicKey(rsa.get(), &openssl_buffer);
  if (length <= 0) {
    LOG(ERROR) << __func__ << ": Failed to encode public key.";
    return false;
  }
  crypto::ScopedOpenSSLBytes scoped_buffer(openssl_buffer);
  public_key_der->assign(reinterpret_cast<char*>(openssl_buffer), length);
  return true;
}

template <typename ReplyProtoType, typename MethodType>
void TpmUtilityV2::SendTpmManagerRequestAndWait(const MethodType& method,
                                                ReplyProtoType* reply_proto) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback = base::Bind(
      [](ReplyProtoType* target, base::WaitableEvent* completion,
         const ReplyProtoType& reply) {
        *target = reply;
        completion->Signal();
      },
      reply_proto, &event);
  tpm_manager_thread_.task_runner()->PostTask(FROM_HERE,
                                              base::Bind(method, callback));
  event.Wait();
}

bool TpmUtilityV2::GetEndorsementPassword(std::string* password) {
  if (endorsement_password_.empty()) {
    if (!CacheTpmState()) {
      return false;
    }
    if (endorsement_password_.empty()) {
      LOG(WARNING) << "TPM endorsement password is not available.";
      return false;
    }
  }
  *password = endorsement_password_;
  return true;
}

bool TpmUtilityV2::GetOwnerPassword(std::string* password) {
  if (owner_password_.empty()) {
    if (!CacheTpmState()) {
      return false;
    }
    if (owner_password_.empty()) {
      LOG(WARNING) << "TPM owner password is not available.";
      return false;
    }
  }
  *password = owner_password_;
  return true;
}

bool TpmUtilityV2::CacheTpmState() {
  tpm_manager::GetTpmStatusReply tpm_status;
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::GetTpmStatus,
                 base::Unretained(tpm_owner_),
                 tpm_manager::GetTpmStatusRequest()),
      &tpm_status);
  if (tpm_status.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to read TPM state from tpm_managerd.";
    return false;
  }
  is_ready_ = tpm_status.enabled() && tpm_status.owned();
  endorsement_password_ = tpm_status.local_data().endorsement_password();
  owner_password_ = tpm_status.local_data().owner_password();
  return true;
}

bool TpmUtilityV2::GetEndorsementKey(KeyType key_type, TPM_HANDLE* key_handle) {
  if (endorsement_keys_.count(key_type) > 0) {
    *key_handle = endorsement_keys_[key_type];
    return true;
  }
  std::string endorsement_password;
  if (!GetEndorsementPassword(&endorsement_password)) {
    return false;
  }
  std::unique_ptr<HmacSession> endorsement_session =
      trunks_factory_->GetHmacSession();
  TPM_RC result =
      endorsement_session->StartUnboundSession(true /* salted */,
                                               false /* enable_encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to setup endorsement session: "
               << trunks::GetErrorString(result);
    return false;
  }
  endorsement_session->SetEntityAuthorizationValue(endorsement_password);
  // Don't fail if the owner password is not available, it may not be needed.
  std::string owner_password;
  GetOwnerPassword(&owner_password);
  std::unique_ptr<HmacSession> owner_session =
      trunks_factory_->GetHmacSession();
  result = owner_session->StartUnboundSession(true /* salted */,
                                              false /* enable_encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to setup owner session: "
               << trunks::GetErrorString(result);
    return false;
  }
  owner_session->SetEntityAuthorizationValue(owner_password);
  trunks::TPM_ALG_ID algorithm =
      (key_type == KEY_TYPE_RSA) ? trunks::TPM_ALG_RSA : trunks::TPM_ALG_ECC;
  result = trunks_utility_->GetEndorsementKey(
      algorithm, endorsement_session->GetDelegate(),
      owner_session->GetDelegate(), key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get endorsement key: "
               << trunks::GetErrorString(result);
    return false;
  }
  endorsement_keys_[key_type] = *key_handle;
  return true;
}

bool TpmUtilityV2::RemoveOwnerDependency() {
  tpm_manager::RemoveOwnerDependencyReply reply;
  tpm_manager::RemoveOwnerDependencyRequest request;
  request.set_owner_dependency(tpm_manager::kTpmOwnerDependency_Attestation);
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::RemoveOwnerDependency,
                 base::Unretained(tpm_owner_), request),
      &reply);
  if (reply.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(WARNING) << __func__ << ": Failed to remove the dependency.";
    return false;
  }
  return true;
}

bool TpmUtilityV2::GetEndorsementPublicKeyModulus(
    KeyType key_type,
    std::string* ekm) {
  if (key_type == KEY_TYPE_RSA) {
    return trunks_utility_->GetPublicRSAEndorsementKeyModulus(ekm) ==
           TPM_RC_SUCCESS;
  }

  LOG(ERROR) << __func__ << ": Not implemented.";
  return false;
}

}  // namespace attestation
