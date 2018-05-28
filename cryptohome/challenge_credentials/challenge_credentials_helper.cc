// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/challenge_credentials/challenge_credentials_helper.h"

#include <base/logging.h>

#include "cryptohome/tpm.h"
#include "cryptohome/username_passkey.h"

using brillo::Blob;
using brillo::SecureBlob;

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

ChallengeCredentialsHelper::~ChallengeCredentialsHelper() = default;

void ChallengeCredentialsHelper::GenerateNew(
    const std::string& account_id,
    const ChallengePublicKeyInfo& public_key_info,
    const Blob& salt,
    const KeyChallengeCallback& key_challenge_callback,
    const GenerateNewCallback& callback) {
  DCHECK(!key_challenge_callback.is_null());
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
    const KeyChallengeCallback& key_challenge_callback,
    const DecryptCallback& callback) {
  DCHECK(!key_challenge_callback.is_null());
  DCHECK(!callback.is_null());
  // TODO(emaxx, https://crbug.com/842791): This should request signature of
  // |salt|, start unsealing session for
  // |keyset_signature_challenge_info.sealed_secret|, request signature of its
  // challenge, complete unsealing, generate credentials from the salt signature
  // and the unsealed secret.
  NOTIMPLEMENTED() << "ChallengeCredentialsHelper::Decrypt";
}

void ChallengeCredentialsHelper::VerifyKey(
    const std::string& account_id,
    const ChallengePublicKeyInfo& public_key_info,
    const KeysetSignatureChallengeInfo& keyset_challenge_info,
    const KeyChallengeCallback& key_challenge_callback,
    const VerifyCallback& callback) {
  DCHECK(!key_challenge_callback.is_null());
  DCHECK(!callback.is_null());
  // TODO(emaxx, https://crbug.com/842791): This should generate a random
  // challenge, request its signature, and verify the signature
  // programmatically.
  NOTIMPLEMENTED() << "ChallengeCredentialsHelper::VerifyKey";
}

}  // namespace cryptohome
