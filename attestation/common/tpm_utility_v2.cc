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

#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <brillo/bind_lambda.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/rsa.h>

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

  bool GetCommandAuthorization(
      const std::string& command_hash,
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

}  // namespace

namespace attestation {

TpmUtilityV2::TpmUtilityV2(tpm_manager::TpmOwnershipInterface* tpm_owner,
                           tpm_manager::TpmNvramInterface* tpm_nvram,
                           trunks::TrunksFactory* trunks_factory)
    : tpm_owner_(tpm_owner),
      tpm_nvram_(tpm_nvram),
      trunks_factory_(trunks_factory) {}

bool TpmUtilityV2::Initialize() {
  if (!tpm_manager_thread_.StartWithOptions(base::Thread::Options(
          base::MessageLoopForIO::TYPE_IO, 0 /* Default stack size. */))) {
    LOG(ERROR) << "Failed to start tpm_manager thread.";
    return false;
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    base::WaitableEvent event(true /* manual_reset */,
                              false /* initially_signaled */);
    tpm_manager_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind([this, &event]() {
          default_tpm_owner_ =
              base::MakeUnique<tpm_manager::TpmOwnershipDBusProxy>();
          default_tpm_nvram_ =
              base::MakeUnique<tpm_manager::TpmNvramDBusProxy>();
          if (default_tpm_owner_->Initialize()) {
            tpm_owner_ = default_tpm_owner_.get();
          }
          if (default_tpm_nvram_->Initialize()) {
            tpm_nvram_ = default_tpm_nvram_.get();
          }
          event.Signal();
        }));
    event.Wait();
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    LOG(ERROR) << "Failed to initialize tpm_managerd clients.";
    return false;
  }
  if (!trunks_factory_) {
    default_trunks_factory_ = base::MakeUnique<trunks::TrunksFactoryImpl>();
    if (!default_trunks_factory_->Initialize()) {
      LOG(ERROR) << "Failed to initialize trunks.";
      return false;
    }
    trunks_factory_ = default_trunks_factory_.get();
  }
  return true;
}

bool TpmUtilityV2::IsTpmReady() {
  if (!is_ready_) {
    tpm_manager::GetTpmStatusReply tpm_status;
    SendTpmManagerRequestAndWait(
        base::Bind(&tpm_manager::TpmOwnershipInterface::GetTpmStatus,
                   base::Unretained(tpm_owner_),
                   tpm_manager::GetTpmStatusRequest()),
        &tpm_status);
    is_ready_ = (tpm_status.enabled() && tpm_status.owned() &&
                 !tpm_status.dictionary_attack_lockout_in_effect());
  }
  return is_ready_;
}

bool TpmUtilityV2::ActivateIdentity(const std::string& delegate_blob,
                                    const std::string& delegate_secret,
                                    const std::string& identity_key_blob,
                                    const std::string& asym_ca_contents,
                                    const std::string& sym_ca_attestation,
                                    std::string* credential) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
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
  std::unique_ptr<trunks::TpmUtility> trunks_utility =
      trunks_factory_->GetTpmUtility();
  TPM_RC result = trunks_utility->LoadKey(identity_key_blob,
                                          empty_password_authorization.get(),
                                          &identity_key_handle);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to load identity key: "
               << trunks::GetErrorString(result);
    return false;
  }
  std::string identity_key_name;
  result = trunks_utility->GetKeyName(identity_key_handle, &identity_key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get identity key name: "
               << trunks::GetErrorString(result);
    return false;
  }

  std::unique_ptr<HmacSession> endorsement_session =
      trunks_factory_->GetHmacSession();
  std::string endorsement_password;
  if (!GetEndorsementPassword(&endorsement_password)) {
    return false;
  }
  result =
      endorsement_session->StartUnboundSession(true /* enable_encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to setup endorsement session: "
               << trunks::GetErrorString(result);
    return false;
  }
  endorsement_session->SetEntityAuthorizationValue(endorsement_password);
  if (!LoadEndorsementKey(key_type)) {
    LOG(ERROR) << __func__ << ": Endorsement key is not available.";
    return false;
  }
  TPM_HANDLE endorsement_key_handle = endorsement_keys_[key_type];
  std::string endorsement_key_name;
  result =
      trunks_utility->GetKeyName(endorsement_key_handle, &endorsement_key_name);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to get endorsement key name: "
               << trunks::GetErrorString(result);
    return false;
  }

  MultipleAuthorizations authorization;
  authorization.AddAuthorizationDelegate(empty_password_authorization.get());
  authorization.AddAuthorizationDelegate(endorsement_session->GetDelegate());
  trunks::_ID_OBJECT identity_object;
  identity_object.integrity_hmac = trunks::Make_TPM2B_DIGEST(credential_mac);
  identity_object.enc_identity = trunks::Make_TPM2B_DIGEST(wrapped_credential);
  std::string identity_object_data;
  trunks::Serialize__ID_OBJECT(identity_object, &identity_object_data);
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
                                      std::string* public_key,
                                      std::string* public_key_tpm_format,
                                      std::string* key_info,
                                      std::string* proof) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::SealToPCR0(const std::string& data,
                              std::string* sealed_data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Unseal(const std::string& sealed_data, std::string* data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::GetEndorsementPublicKey(KeyType key_type,
                                           std::string* public_key) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::GetEndorsementCertificate(KeyType key_type,
                                             std::string* certificate) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Unbind(const std::string& key_blob,
                          const std::string& bound_data,
                          std::string* data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Sign(const std::string& key_blob,
                        const std::string& data_to_sign,
                        std::string* signature) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::CreateRestrictedKey(KeyType key_type,
                                       KeyUsage key_usage,
                                       std::string* public_key,
                                       std::string* private_key_blob) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::QuotePCR(int pcr_index,
                            const std::string& key_blob,
                            std::string* quoted_pcr_value,
                            std::string* quoted_data,
                            std::string* quote) {
  LOG(ERROR) << __func__ << ": Not implemented.";
  return false;
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
  base::WaitableEvent event(true /* manual_reset */,
                            false /* initially_signaled */);
  auto callback =
      base::Bind([reply_proto, &event](const ReplyProtoType& reply) {
        *reply_proto = reply;
        event.Signal();
      });
  tpm_manager_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind([this, &method, &callback]() { method.Run(callback); }));
  event.Wait();
}

bool TpmUtilityV2::GetEndorsementPassword(std::string* password) {
  if (endorsement_password_.empty()) {
    tpm_manager::GetTpmStatusReply tpm_status;
    SendTpmManagerRequestAndWait(
        base::Bind(&tpm_manager::TpmOwnershipInterface::GetTpmStatus,
                   base::Unretained(tpm_owner_),
                   tpm_manager::GetTpmStatusRequest()),
        &tpm_status);
    if (tpm_status.status() != tpm_manager::STATUS_SUCCESS ||
        tpm_status.local_data().endorsement_password().empty()) {
      return false;
    }
    endorsement_password_ = tpm_status.local_data().endorsement_password();
  }
  *password = endorsement_password_;
  return true;
}

bool TpmUtilityV2::LoadEndorsementKey(KeyType key_type) {
  // TODO: implement
  return true;
}

}  // namespace attestation
