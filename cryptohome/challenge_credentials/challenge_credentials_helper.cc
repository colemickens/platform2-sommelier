// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/challenge_credentials/challenge_credentials_helper.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>

#include "cryptohome/challenge_credentials/challenge_credentials_decrypt_operation.h"
#include "cryptohome/challenge_credentials/challenge_credentials_generate_new_operation.h"
#include "cryptohome/challenge_credentials/challenge_credentials_operation.h"
#include "cryptohome/credentials.h"
#include "cryptohome/key_challenge_service.h"
#include "cryptohome/signature_sealing_backend.h"
#include "cryptohome/tpm.h"

using brillo::Blob;

namespace cryptohome {

ChallengeCredentialsHelper::ChallengeCredentialsHelper(
    Tpm* tpm,
    const Blob& delegate_blob,
    const Blob& delegate_secret)
    : tpm_(tpm),
      delegate_blob_(delegate_blob),
      delegate_secret_(delegate_secret) {
  DCHECK(tpm_);
}

ChallengeCredentialsHelper::~ChallengeCredentialsHelper() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ChallengeCredentialsHelper::GenerateNew(
    const std::string& account_id,
    const KeyData& key_data,
    const std::vector<std::map<uint32_t, Blob>>& pcr_restrictions,
    std::unique_ptr<KeyChallengeService> key_challenge_service,
    GenerateNewCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(key_data.type(), KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  DCHECK(!callback.is_null());
  CancelRunningOperation();
  DCHECK(!key_challenge_service_);
  key_challenge_service_ = std::move(key_challenge_service);
  operation_ = std::make_unique<ChallengeCredentialsGenerateNewOperation>(
      key_challenge_service_.get(), tpm_, delegate_blob_, delegate_secret_,
      account_id, key_data, pcr_restrictions,
      base::BindOnce(&ChallengeCredentialsHelper::OnGenerateNewCompleted,
                     base::Unretained(this), std::move(callback)));
  operation_->Start();
}

void ChallengeCredentialsHelper::Decrypt(
    const std::string& account_id,
    const KeyData& key_data,
    const KeysetSignatureChallengeInfo& keyset_challenge_info,
    std::unique_ptr<KeyChallengeService> key_challenge_service,
    DecryptCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(key_data.type(), KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  DCHECK(!callback.is_null());
  CancelRunningOperation();
  DCHECK(!key_challenge_service_);
  key_challenge_service_ = std::move(key_challenge_service);
  operation_ = std::make_unique<ChallengeCredentialsDecryptOperation>(
      key_challenge_service_.get(), tpm_, delegate_blob_, delegate_secret_,
      account_id, key_data, keyset_challenge_info,
      base::BindOnce(&ChallengeCredentialsHelper::OnDecryptCompleted,
                     base::Unretained(this), std::move(callback)));
  operation_->Start();
}

void ChallengeCredentialsHelper::VerifyKey(
    const std::string& account_id,
    const KeyData& key_data,
    const KeysetSignatureChallengeInfo& keyset_challenge_info,
    std::unique_ptr<KeyChallengeService> key_challenge_service,
    VerifyKeyCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(key_data.type(), KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  DCHECK(!callback.is_null());
  CancelRunningOperation();
  DCHECK(!key_challenge_service_);
  // TODO(emaxx, https://crbug.com/842791): This should generate a random
  // challenge, request its signature, and verify the signature
  // programmatically.
  NOTIMPLEMENTED() << "ChallengeCredentialsHelper::VerifyKey";
}

void ChallengeCredentialsHelper::CancelRunningOperation() {
  // Destroy the previous Operation before instantiating a new one, to keep the
  // resource usage constrained (for example, there must be only one instance of
  // SignatureSealingBackend::UnsealingSession at a time).
  if (operation_) {
    DLOG(INFO) << "Cancelling an old challenge-response credentials operation";
    operation_->Abort();
    operation_.reset();
    // It's illegal for the consumer code to request a new operation in
    // immediate response to completion of a previous one.
    DCHECK(!operation_);

    key_challenge_service_.reset();
  }
}

void ChallengeCredentialsHelper::OnGenerateNewCompleted(
    GenerateNewCallback original_callback,
    std::unique_ptr<Credentials> credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CancelRunningOperation();
  std::move(original_callback).Run(std::move(credentials));
}

void ChallengeCredentialsHelper::OnDecryptCompleted(
    DecryptCallback original_callback,
    Tpm::TpmRetryAction /* retry_action */,
    std::unique_ptr<Credentials> credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CancelRunningOperation();
  std::move(original_callback).Run(std::move(credentials));
}

}  // namespace cryptohome
