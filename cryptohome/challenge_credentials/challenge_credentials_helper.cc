// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/challenge_credentials/challenge_credentials_helper.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>

#include "cryptohome/challenge_credentials/challenge_credentials_operation.h"
#include "cryptohome/tpm.h"
#include "cryptohome/username_passkey.h"

using brillo::Blob;

namespace cryptohome {

ChallengeCredentialsHelper::ChallengeCredentialsHelper(
    Tpm* tpm,
    KeyChallengeService* key_challenge_service,
    const Blob& delegate_blob,
    const Blob& delegate_secret)
    : tpm_(tpm),
      key_challenge_service_(key_challenge_service),
      delegate_blob_(delegate_blob),
      delegate_secret_(delegate_secret) {
  DCHECK(tpm_);
}

ChallengeCredentialsHelper::~ChallengeCredentialsHelper() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ChallengeCredentialsHelper::GenerateNew(
    const std::string& account_id,
    const ChallengePublicKeyInfo& public_key_info,
    const Blob& salt,
    const GenerateNewCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());
  // TODO(emaxx, https://crbug.com/842791): This should request signature of
  // |salt|, create a sealed secret, start unsealing session for unsealing it,
  // request signature of its challenge, complete unsealing, generate
  // credentials from the salt signature and the unsealed secret.
  NOTIMPLEMENTED() << "ChallengeCredentialsHelper::GenerateNew";
}

void ChallengeCredentialsHelper::Decrypt(
    const std::string& account_id,
    const ChallengePublicKeyInfo& public_key_info,
    const Blob& salt,
    const KeysetSignatureChallengeInfo& keyset_challenge_info,
    const DecryptCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());
  CancelRunningOperation();
  operation_ = std::make_unique<ChallengeCredentialsDecryptOperation>(
      key_challenge_service_, tpm_, delegate_blob_, delegate_secret_,
      account_id, public_key_info, salt, keyset_challenge_info,
      nullptr /* salt_signature */,
      base::Bind(&ChallengeCredentialsHelper::OnDecryptCompleted,
                 base::Unretained(this), callback));
  operation_->Start();
}

void ChallengeCredentialsHelper::VerifyKey(
    const std::string& account_id,
    const ChallengePublicKeyInfo& public_key_info,
    const KeysetSignatureChallengeInfo& keyset_challenge_info,
    const VerifyKeyCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());
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
  }
}

void ChallengeCredentialsHelper::OnDecryptCompleted(
    const DecryptCallback& original_callback,
    std::unique_ptr<UsernamePasskey> username_passkey) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CancelRunningOperation();
  original_callback.Run(std::move(username_passkey));
}

}  // namespace cryptohome
