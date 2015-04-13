// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/trunks_factory_for_test.h"

#include <gmock/gmock.h>

#include "trunks/authorization_delegate.h"
#include "trunks/authorization_session.h"
#include "trunks/mock_authorization_session.h"
#include "trunks/mock_tpm.h"
#include "trunks/mock_tpm_state.h"
#include "trunks/mock_tpm_utility.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_state.h"
#include "trunks/tpm_utility.h"

using testing::NiceMock;

namespace trunks {

// Forwards all calls to a target instance.
class TpmStateForwarder : public TpmState {
 public:
  explicit TpmStateForwarder(TpmState* target) : target_(target) {}
  ~TpmStateForwarder() override {}

  TPM_RC Initialize() override {
    return target_->Initialize();
  }

  bool IsOwnerPasswordSet() override {
    return target_->IsOwnerPasswordSet();
  }

  bool IsEndorsementPasswordSet() override {
    return target_->IsEndorsementPasswordSet();
  }

  bool IsLockoutPasswordSet() override {
    return target_->IsLockoutPasswordSet();
  }

  bool IsInLockout() override {
    return target_->IsInLockout();
  }

  bool IsPlatformHierarchyEnabled() override {
    return target_->IsPlatformHierarchyEnabled();
  }

  bool WasShutdownOrderly() override {
    return target_->WasShutdownOrderly();
  }

  bool IsRSASupported() override {
    return target_->IsRSASupported();
  }

  bool IsECCSupported() override {
    return target_->IsECCSupported();
  }

 private:
  TpmState* target_;
};

// Forwards all calls to a target instance.
class TpmUtilityForwarder : public TpmUtility {
 public:
  explicit TpmUtilityForwarder(TpmUtility* target) : target_(target) {}
  ~TpmUtilityForwarder() override {}

  TPM_RC Startup() override {
    return target_->Startup();
  }

  TPM_RC Clear() override {
    return target_->Clear();
  }

  void Shutdown() override {
    return target_->Shutdown();
  }

  TPM_RC InitializeTpm() override {
    return target_->InitializeTpm();
  }

  TPM_RC TakeOwnership(const std::string& owner_password,
                       const std::string& endorsement_password,
                       const std::string& lockout_password) override {
    return target_->TakeOwnership(owner_password,
                                  endorsement_password,
                                  lockout_password);
  }

  TPM_RC StirRandom(const std::string& entropy_data,
                    AuthorizationDelegate* delegate) override {
    return target_->StirRandom(entropy_data, delegate);
  }

  TPM_RC GenerateRandom(size_t num_bytes,
                        AuthorizationDelegate* delegate,
                        std::string* random_data) override {
    return target_->GenerateRandom(num_bytes, delegate, random_data);
  }

  TPM_RC ExtendPCR(int pcr_index,
                   const std::string& extend_data,
                   AuthorizationDelegate* delegate) override {
    return target_->ExtendPCR(pcr_index, extend_data, delegate);
  }

  TPM_RC ReadPCR(int pcr_index, std::string* pcr_value) override {
    return target_->ReadPCR(pcr_index, pcr_value);
  }

  TPM_RC AsymmetricEncrypt(TPM_HANDLE key_handle,
                           TPM_ALG_ID scheme,
                           TPM_ALG_ID hash_alg,
                           const std::string& plaintext,
                           AuthorizationDelegate* delegate,
                           std::string* ciphertext) override {
    return target_->AsymmetricEncrypt(key_handle,
                                      scheme,
                                      hash_alg,
                                      plaintext,
                                      delegate,
                                      ciphertext);
  }

  TPM_RC AsymmetricDecrypt(TPM_HANDLE key_handle,
                           TPM_ALG_ID scheme,
                           TPM_ALG_ID hash_alg,
                           const std::string& ciphertext,
                           AuthorizationDelegate* delegate,
                           std::string* plaintext) override {
    return target_->AsymmetricDecrypt(key_handle,
                                      scheme,
                                      hash_alg,
                                      ciphertext,
                                      delegate,
                                      plaintext);
  }

  TPM_RC Sign(TPM_HANDLE key_handle,
              TPM_ALG_ID scheme,
              TPM_ALG_ID hash_alg,
              const std::string& plaintext,
              AuthorizationDelegate* delegate,
              std::string* signature) override {
    return target_->Sign(key_handle,
                         scheme,
                         hash_alg,
                         plaintext,
                         delegate,
                         signature);
  }

  TPM_RC Verify(TPM_HANDLE key_handle,
                TPM_ALG_ID scheme,
                TPM_ALG_ID hash_alg,
                const std::string& plaintext,
                const std::string& signature) override {
    return target_->Verify(key_handle, scheme, hash_alg, plaintext, signature);
  }

  TPM_RC ChangeKeyAuthorizationData(TPM_HANDLE key_handle,
                                    const std::string& new_password,
                                    AuthorizationDelegate* delegate,
                                    std::string* key_blob) override {
    return target_->ChangeKeyAuthorizationData(key_handle,
                                               new_password,
                                               delegate,
                                               key_blob);
  }

  TPM_RC ImportRSAKey(AsymmetricKeyUsage key_type,
                      const std::string& modulus,
                      uint32_t public_exponent,
                      const std::string& prime_factor,
                      const std::string& password,
                      AuthorizationDelegate* delegate,
                      std::string* key_blob) override {
    return target_->ImportRSAKey(key_type, modulus, public_exponent,
                                 prime_factor, password, delegate, key_blob);
  }

  TPM_RC CreateAndLoadRSAKey(AsymmetricKeyUsage key_type,
                             const std::string& password,
                             AuthorizationDelegate* delegate,
                             TPM_HANDLE* key_handle,
                             std::string* key_blob) override {
    return target_->CreateAndLoadRSAKey(key_type, password, delegate,
                                        key_handle, key_blob);
  }

  TPM_RC CreateRSAKeyPair(AsymmetricKeyUsage key_type,
                          int modulus_bits,
                          uint32_t public_exponent,
                          const std::string& password,
                          AuthorizationDelegate* delegate,
                          std::string* key_blob) override {
    return target_->CreateRSAKeyPair(key_type, modulus_bits, public_exponent,
                                     password, delegate, key_blob);
  }

  TPM_RC LoadKey(const std::string& key_blob,
                 AuthorizationDelegate* delegate,
                 TPM_HANDLE* key_handle) override {
    return target_->LoadKey(key_blob, delegate, key_handle);
  }

  TPM_RC GetKeyName(TPM_HANDLE handle, std::string* name) override {
    return target_->GetKeyName(handle, name);
  }

  TPM_RC GetKeyPublicArea(TPM_HANDLE handle,
                          TPMT_PUBLIC* public_data) override {
    return target_->GetKeyPublicArea(handle, public_data);
  }

  TPM_RC DefineNVSpace(uint32_t index,
                       size_t num_bytes,
                       AuthorizationDelegate* delegate) override {
    return target_->DefineNVSpace(index, num_bytes, delegate);
  }

  TPM_RC DestroyNVSpace(uint32_t index,
                        AuthorizationDelegate* delegate) override {
    return target_->DestroyNVSpace(index, delegate);
  }

  TPM_RC LockNVSpace(uint32_t index,
                     AuthorizationDelegate* delegate) override {
    return target_->LockNVSpace(index, delegate);
  }

  TPM_RC WriteNVSpace(uint32_t index,
                      uint32_t offset,
                      const std::string& nvram_data,
                      AuthorizationDelegate* delegate) override {
    return target_->WriteNVSpace(index, offset, nvram_data, delegate);
  }

  TPM_RC ReadNVSpace(uint32_t index,
                     uint32_t offset,
                     size_t num_bytes,
                     std::string* nvram_data,
                     AuthorizationDelegate* delegate) override {
    return target_->ReadNVSpace(index, offset, num_bytes, nvram_data, delegate);
  }

  TPM_RC GetNVSpaceName(uint32_t index, std::string* name) override {
    return target_->GetNVSpaceName(index, name);
  }

  TPM_RC GetNVSpacePublicArea(uint32_t index,
                              TPMS_NV_PUBLIC* public_data) override {
    return target_->GetNVSpacePublicArea(index, public_data);
  }

 private:
  TpmUtility* target_;
};

// Forwards all calls to a target instance.
class AuthorizationDelegateForwarder : public AuthorizationDelegate {
 public:
  explicit AuthorizationDelegateForwarder(AuthorizationDelegate* target)
      : target_(target) {}
  ~AuthorizationDelegateForwarder() override {}

  bool GetCommandAuthorization(const std::string& command_hash,
                               bool is_command_parameter_encryption_possible,
                               bool is_response_parameter_encryption_possible,
                               std::string* authorization) override {
    return target_->GetCommandAuthorization(
        command_hash,
        is_command_parameter_encryption_possible,
        is_response_parameter_encryption_possible,
        authorization);
  }

  bool CheckResponseAuthorization(const std::string& response_hash,
                                  const std::string& authorization) override {
    return target_->CheckResponseAuthorization(response_hash, authorization);
  }

  bool EncryptCommandParameter(std::string* parameter) override {
    return target_->EncryptCommandParameter(parameter);
  }

  bool DecryptResponseParameter(std::string* parameter) override {
    return target_->DecryptResponseParameter(parameter);
  }

 private:
  AuthorizationDelegate* target_;
};

// Forwards all calls to a target instance.
class AuthorizationSessionForwarder : public AuthorizationSession {
 public:
  explicit AuthorizationSessionForwarder(AuthorizationSession* target)
      : target_(target) {}
  ~AuthorizationSessionForwarder() override {}

  AuthorizationDelegate* GetDelegate() override {
    return target_->GetDelegate();
  }

  TPM_RC StartBoundSession(
      TPMI_DH_ENTITY bind_entity,
      const std::string& bind_authorization_value,
      bool enable_encryption) override {
    return target_->StartBoundSession(bind_entity,
                                      bind_authorization_value,
                                      enable_encryption);
  }

  TPM_RC StartUnboundSession(bool enable_encryption) override {
    return target_->StartUnboundSession(enable_encryption);
  }

  void SetEntityAuthorizationValue(const std::string& value) override {
    return target_->SetEntityAuthorizationValue(value);
  }

  void SetFutureAuthorizationValue(const std::string& value) override {
    return target_->SetFutureAuthorizationValue(value);
  }

 private:
  AuthorizationSession* target_;
};

TrunksFactoryForTest::TrunksFactoryForTest()
    : default_tpm_(new NiceMock<MockTpm>()),
      tpm_(default_tpm_.get()),
      default_tpm_state_(new NiceMock<MockTpmState>()),
      tpm_state_(default_tpm_state_.get()),
      default_tpm_utility_(new NiceMock<MockTpmUtility>()),
      tpm_utility_(default_tpm_utility_.get()),
      default_authorization_delegate_(new PasswordAuthorizationDelegate("")),
      password_authorization_delegate_(default_authorization_delegate_.get()),
      default_authorization_session_(new NiceMock<MockAuthorizationSession>()),
      authorization_session_(default_authorization_session_.get()) {
}

TrunksFactoryForTest::~TrunksFactoryForTest() {
}

Tpm* TrunksFactoryForTest::GetTpm() const {
  return tpm_;
}

scoped_ptr<TpmState> TrunksFactoryForTest::GetTpmState() const {
  return scoped_ptr<TpmState>(new TpmStateForwarder(tpm_state_));
}

scoped_ptr<TpmUtility> TrunksFactoryForTest::GetTpmUtility() const {
  return scoped_ptr<TpmUtility>(new TpmUtilityForwarder(tpm_utility_));
}

scoped_ptr<AuthorizationDelegate>
    TrunksFactoryForTest::GetPasswordAuthorization(
        const std::string& password) const {
  return scoped_ptr<AuthorizationDelegate>(
      new AuthorizationDelegateForwarder(password_authorization_delegate_));
}

scoped_ptr<AuthorizationSession>
    TrunksFactoryForTest::GetHmacAuthorizationSession() const {
  return scoped_ptr<AuthorizationSession>(
      new AuthorizationSessionForwarder(authorization_session_));
}

}  // namespace trunks
