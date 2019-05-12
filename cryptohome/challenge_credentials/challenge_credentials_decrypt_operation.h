// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_DECRYPT_OPERATION_H_
#define CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_DECRYPT_OPERATION_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <brillo/secure_blob.h>

#include "cryptohome/challenge_credentials/challenge_credentials_operation.h"
#include "cryptohome/signature_sealing_backend.h"

#include "key.pb.h"           // NOLINT(build/include)
#include "vault_keyset.pb.h"  // NOLINT(build/include)

namespace cryptohome {

class Credentials;
class KeyChallengeService;
class Tpm;

// This operation decrypts the credentials for the given user and the referenced
// cryptographic key. This operation involves making challenge request(s)
// against the specified key.
//
// This class is not expected to be used directly by client code; instead,
// methods of ChallengeCredentialsHelper should be called.
class ChallengeCredentialsDecryptOperation final
    : public ChallengeCredentialsOperation {
 public:
  using KeysetSignatureChallengeInfo =
      SerializedVaultKeyset_SignatureChallengeInfo;

  // If the operation succeeds, |credentials| will contain the decrypted
  // credentials that can be used for decryption of the user's vault keyset.
  using CompletionCallback =
      base::OnceCallback<void(std::unique_ptr<Credentials> credentials)>;

  // |key_challenge_service| is a non-owned pointer which must outlive the
  // created instance.
  // |key_data| must have the |KEY_TYPE_CHALLENGE_RESPONSE| type.
  // |keyset_challenge_info| contains the encrypted representation of secrets.
  // The result is reported via |completion_callback|.
  ChallengeCredentialsDecryptOperation(
      KeyChallengeService* key_challenge_service,
      Tpm* tpm,
      const brillo::Blob& delegate_blob,
      const brillo::Blob& delegate_secret,
      const std::string& account_id,
      const KeyData& key_data,
      const KeysetSignatureChallengeInfo& keyset_challenge_info,
      CompletionCallback completion_callback);

  ~ChallengeCredentialsDecryptOperation() override;

  // ChallengeCredentialsOperation:
  void Start() override;
  void Abort() override;

 private:
  // Starts the processing. Returns |false| on fatal error.
  bool StartProcessing();

  // Makes a challenge request with the salt. Returns |false| on fatal error.
  bool StartProcessingSalt();

  // Begins unsealing the secret, and makes a challenge request for unsealing
  // it. Returns |false| on fatal error.
  bool StartProcessingSealedSecret();

  // Called when signature for the salt is received.
  void OnSaltChallengeResponse(std::unique_ptr<brillo::Blob> salt_signature);

  // Called when signature for the unsealing challenge is received.
  void OnUnsealingChallengeResponse(
      std::unique_ptr<brillo::Blob> challenge_signature);

  // Generates the result if all necessary challenges are completed.
  void ProceedIfChallengesDone();

  Tpm* const tpm_;
  const brillo::Blob delegate_blob_;
  const brillo::Blob delegate_secret_;
  const std::string account_id_;
  const KeyData key_data_;
  const KeysetSignatureChallengeInfo keyset_challenge_info_;
  std::unique_ptr<brillo::Blob> salt_signature_;
  CompletionCallback completion_callback_;
  SignatureSealingBackend* const signature_sealing_backend_;
  ChallengePublicKeyInfo public_key_info_;
  std::unique_ptr<SignatureSealingBackend::UnsealingSession> unsealing_session_;
  std::unique_ptr<brillo::SecureBlob> unsealed_secret_;
  base::WeakPtrFactory<ChallengeCredentialsDecryptOperation> weak_ptr_factory_{
      this};
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_DECRYPT_OPERATION_H_
