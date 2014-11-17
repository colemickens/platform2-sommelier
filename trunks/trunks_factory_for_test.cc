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
#include "trunks/null_authorization_delegate.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_state.h"
#include "trunks/tpm_utility.h"

using testing::NiceMock;

namespace trunks {

// Forwards all calls to a target instance.
class TpmStateForwarder : public TpmState {
 public:
  explicit TpmStateForwarder(TpmState* target) : target_(target) {}
  virtual ~TpmStateForwarder() {}

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

 private:
  TpmState* target_;
};

// Forwards all calls to a target instance.
class TpmUtilityForwarder : public TpmUtility {
 public:
  explicit TpmUtilityForwarder(TpmUtility* target) : target_(target) {}
  virtual ~TpmUtilityForwarder() {}

  TPM_RC Startup() override {
    return target_->Startup();
  }

  TPM_RC Clear() override {
    return target_->Clear();
  }

  TPM_RC InitializeTpm() override {
    return target_->InitializeTpm();
  }

  TPM_RC StirRandom(const std::string& entropy_data) override {
    return target_->StirRandom(entropy_data);
  }

  TPM_RC GenerateRandom(int num_bytes, std::string* random_data) override {
    return target_->GenerateRandom(num_bytes, random_data);
  }

  TPM_RC ExtendPCR(int pcr_index, const std::string& extend_data) override {
    return target_->ExtendPCR(pcr_index, extend_data);
  }

  TPM_RC ReadPCR(int pcr_index, std::string* pcr_value) override {
    return target_->ReadPCR(pcr_index, pcr_value);
  }

  TPM_RC TakeOwnership(const std::string& owner_password,
                       const std::string& endorsement_password,
                       const std::string& lockout_password) override {
    return target_->TakeOwnership(owner_password,
                                  endorsement_password,
                                  lockout_password);
  }

  TPM_RC CreateStorageRootKeys(const std::string& owner_password) override {
    return target_->CreateStorageRootKeys(owner_password);
  }

  TPM_RC AsymmetricEncrypt(TPM_HANDLE key_handle,
                           TPM_ALG_ID scheme,
                           TPM_ALG_ID hash_alg,
                           const std::string& plaintext,
                           std::string* ciphertext) override {
    return target_->AsymmetricEncrypt(key_handle,
                                      scheme,
                                      hash_alg,
                                      plaintext,
                                      ciphertext);
  }

  TPM_RC AsymmetricDecrypt(TPM_HANDLE key_handle,
                           TPM_ALG_ID scheme,
                           TPM_ALG_ID hash_alg,
                           const std::string& password,
                           const std::string& ciphertext,
                           std::string* plaintext) override {
    return target_->AsymmetricDecrypt(key_handle,
                                      scheme,
                                      hash_alg,
                                      password,
                                      ciphertext,
                                      plaintext);
  }

  TPM_RC Sign(TPM_HANDLE key_handle,
              TPM_ALG_ID scheme,
              TPM_ALG_ID hash_alg,
              const std::string& password,
              const std::string& digest,
              std::string* signature) override {
    return target_->Sign(key_handle,
                         scheme,
                         hash_alg,
                         password,
                         digest,
                         signature);
  }

  TPM_RC Verify(TPM_HANDLE key_handle,
                TPM_ALG_ID scheme,
                TPM_ALG_ID hash_alg,
                const std::string& digest,
                const std::string& signature) override {
    return target_->Verify(key_handle, scheme, hash_alg, digest, signature);
  }

  TPM_RC CreateRSAKey(AsymmetricKeyUsage key_type,
                      const std::string& password,
                      TPM_HANDLE* key_handle) override {
    return target_->CreateRSAKey(key_type, password, key_handle);
  }

 private:
  TpmUtility* target_;
};

// Forwards all calls to a target instance.
class AuthorizationDelegateForwarder : public AuthorizationDelegate {
 public:
  explicit AuthorizationDelegateForwarder(AuthorizationDelegate* target)
      : target_(target) {}
  virtual ~AuthorizationDelegateForwarder() {}

  bool GetCommandAuthorization(const std::string& command_hash,
                               std::string* authorization) override {
    return target_->GetCommandAuthorization(command_hash, authorization);
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
  virtual ~AuthorizationSessionForwarder() {}

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
      default_authorization_delegate_(new NullAuthorizationDelegate()),
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
    TrunksFactoryForTest::GetAuthorizationSession() const {
  return scoped_ptr<AuthorizationSession>(
      new AuthorizationSessionForwarder(authorization_session_));
}

}  // namespace trunks
