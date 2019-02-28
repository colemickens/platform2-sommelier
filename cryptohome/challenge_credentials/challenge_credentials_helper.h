// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_HELPER_H_
#define CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_HELPER_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/threading/thread_checker.h>
#include <brillo/secure_blob.h>

#include "cryptohome/challenge_credentials/challenge_credentials_decrypt_operation.h"

#include "key.pb.h"           // NOLINT(build/include)
#include "rpc.pb.h"           // NOLINT(build/include)
#include "vault_keyset.pb.h"  // NOLINT(build/include)

namespace cryptohome {

class ChallengeCredentialsOperation;
class KeyChallengeService;
class Tpm;
class UsernamePasskey;

// This class provides generation of credentials for challenge-protected vault
// keysets, and verification of key validity for such keysets.
//
// It's expected that the consumer code instantiates a single instance during
// the whole daemon lifetime. This allows to keep the resource usage
// constrained, e.g., to have a limited number of active TPM sessions.
//
// NOTE: This object supports only one operation (GenerateNew() / Decrypt() /
// VerifyKey()) at a time. Starting a new operation before the previous one
// completes will lead to cancellation of the previous operation (i.e., the
// old operation will complete with a failure).
//
// This class must be used on a single thread only.
class ChallengeCredentialsHelper final {
 public:
  using KeysetSignatureChallengeInfo =
      SerializedVaultKeyset_SignatureChallengeInfo;

  // This callback reports result of a GenerateNew() call.
  //
  // If the operation succeeds, |username_passkey| will contain the freshly
  // generated credentials that should be used for encrypting the new vault
  // keyset, with the challenge_credentials_keyset_info() field containing the
  // data to be stored in the created vault keyset.
  // If the operation fails, the argument will be null.
  using GenerateNewCallback = base::Callback<void(
      std::unique_ptr<UsernamePasskey> /* username_passkey */)>;

  // This callback reports result of a Decrypt() call.
  //
  // If the operation succeeds, |username_passkey| will contain the built
  // credentials that should be used for decrypting the user's vault keyset.
  // If the operation fails, the argument will be null.
  using DecryptCallback = base::Callback<void(
      std::unique_ptr<UsernamePasskey> /* username_passkey */)>;

  // This callback reports result of a VerifyKey() call.
  //
  // The |is_key_valid| argument will be true iff the operation succeeds and
  // the provided key is valid for decryption of the given vault keyset.
  using VerifyKeyCallback = base::Callback<void(bool /* is_key_valid */)>;

  // |tpm| is a non-owned pointer that must stay valid for the whole lifetime of
  // the created object.
  // |delegate_blob| and |delegate_secret| should correspond to a TPM delegate
  // that allows doing signature-sealing operations (currently used only on TPM
  // 1.2).
  ChallengeCredentialsHelper(Tpm* tpm,
                             const brillo::Blob& delegate_blob,
                             const brillo::Blob& delegate_secret);

  ~ChallengeCredentialsHelper();

  // Generates and returns fresh random-based credentials for the given user
  // and the referenced cryptographic key, and also returns the encrypted
  // (challenge-protected) representation of the created secrets that should
  // be stored in the created vault keyset. This operation may involve making
  // challenge request(s) against the specified key.
  //
  // |public_key_info| describes the cryptographic key.
  // The result is reported via |callback|.
  void GenerateNew(const std::string& account_id,
                   const ChallengePublicKeyInfo& public_key_info,
                   std::unique_ptr<KeyChallengeService> key_challenge_service,
                   const GenerateNewCallback& callback);

  // Builds credentials for the given user, based on the encrypted
  // (challenge-protected) representation of the previously created secrets. The
  // referred cryptographic key should be the same as the one used for the
  // secrets generation via GenerateNew(); although a difference in the key's
  // supported algorithms may be tolerated in some cases. This operation
  // involves making challenge request(s) against the key.
  //
  // |public_key_info| describes the cryptographic key.
  // |keyset_challenge_info| is the encrypted representation of secrets as
  // created via GenerateNew().
  // The result is reported via |callback|.
  void Decrypt(const std::string& account_id,
               const ChallengePublicKeyInfo& public_key_info,
               const KeysetSignatureChallengeInfo& keyset_challenge_info,
               std::unique_ptr<KeyChallengeService> key_challenge_service,
               const DecryptCallback& callback);

  // Verifies whether the specified cryptographic key may be used to decrypt
  // the specified vault keyset. This operation involves cryptographic
  // challenge(s) of the specified key. This method is intended as a
  // lightweight analog of Decrypt() for cases where the actual credentials
  // aren't needed.
  //
  // |public_key_info| describes the cryptographic key.
  // |keyset_challenge_info| is the encrypted representation of secrets as
  // created via GenerateNew().
  // The result is reported via |callback|.
  void VerifyKey(const std::string& account_id,
                 const ChallengePublicKeyInfo& public_key_info,
                 const KeysetSignatureChallengeInfo& keyset_challenge_info,
                 std::unique_ptr<KeyChallengeService> key_challenge_service,
                 const VerifyKeyCallback& callback);

 private:
  // Aborts the currently running operation, if any, and destroys all resources
  // associated with it.
  void CancelRunningOperation();

  // Wrapper for the completion callback of Decrypt(). Cleans up resources
  // associated with the operation and forwards results to the original
  // callback.
  void OnDecryptCompleted(const DecryptCallback& original_callback,
                          std::unique_ptr<UsernamePasskey> username_passkey);

  // Non-owned.
  Tpm* const tpm_;
  // TPM delegate that was passed to the constructor.
  const brillo::Blob delegate_blob_;
  const brillo::Blob delegate_secret_;
  // The key challenge service used for the currently running operation, if any.
  std::unique_ptr<KeyChallengeService> key_challenge_service_;
  // The state of the currently running operation, if any.
  std::unique_ptr<ChallengeCredentialsOperation> operation_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ChallengeCredentialsHelper);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_HELPER_H_
