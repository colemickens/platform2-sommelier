// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_HELPER_H_
#define CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_HELPER_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <brillo/secure_blob.h>

#include "key.pb.h"           // NOLINT(build/include)
#include "rpc.pb.h"           // NOLINT(build/include)
#include "vault_keyset.pb.h"  // NOLINT(build/include)

namespace cryptohome {

class Tpm;
class UsernamePasskey;

// This class provides generation of credentials for challenge-protected vault
// keysets, and verification of key validity for such keysets.
//
// It's expected that the consumer code instantiates a single instance of this
// class and keeps it for performing of all operations. This allows to keep
// the resource usage imposed by the operations constrained, e.g., to have a
// limited number of active TPM sessions.
//
// NOTE: This object supports only one operation (GenerateNew() / Decrypt() /
// Verify()) at a time. Starting a new operation before the previous one
// completes will lead to cancellation of the previous operation (i.e., the
// old operation will complete with a failure).
class ChallengeCredentialsHelper final {
 public:
  using KeysetSignatureChallengeInfo =
      SerializedVaultKeyset_SignatureChallengeInfo;

  // This callback is called with a response for a challenge request made via
  // |KeyChallengeCallback|. In real use cases, this callback delivers the
  // response of an IPC request made to the service that talks to the
  // cryptographic token with the challenged key.
  //
  // In case of error, |response| will be null; otherwise, it will contain the
  // challenge response data.
  using KeyChallengeResponseCallback = base::Callback<void(
      std::unique_ptr<KeyChallengeResponse> /* response */)>;

  // This callback performs a challenge request against the specified
  // cryptographic key. In real use cases, this callback will make an IPC
  // request to the server that talks to the cryptographic token with the
  // challenged key.
  //
  // The challenge data is passed via |key_challenge_request|, and
  // |account_id| specifies the account whom the vault keyset in question
  // belongs. The result is reported via |response_callback|.
  using KeyChallengeCallback = base::Callback<void(
      const AccountIdentifier& /* account_id */,
      std::unique_ptr<KeyChallengeRequest> /* key_challenge_request */,
      const KeyChallengeResponseCallback& /* response_callback */)>;

  // This callback reports results of a GenerateNew() call.
  //
  // If the operation succeeds, |username_passkey| will contain the freshly
  // generated credentials that should be used for encrypting the new vault
  // keyset, and |keyset_challenge_info| will be the data to be stored in the
  // created vault keyset. If the operation fails, both arguments will be
  // null.
  using GenerateNewCallback = base::Callback<void(
      std::unique_ptr<UsernamePasskey> /* username_passkey */,
      std::unique_ptr<KeysetSignatureChallengeInfo>
      /* keyset_challenge_info */)>;

  // This callback reports result of a Decrypt() call.
  //
  // If the operation succeeds, |username_passkey| will contain the built
  // credentials that should be used for decrypting the user's vault keyset.
  // If the operation fails, the argument will be null.
  using DecryptCallback = base::Callback<void(
      std::unique_ptr<UsernamePasskey> /* username_passkey */)>;

  // This callback reports result of a Verify() call.
  //
  // The |is_key_valid| argument will be true iff the operation succeeds and
  // the provided key is valid for decryption of the given vault keyset.
  using VerifyCallback = base::Callback<void(bool /* is_key_valid */)>;

  // |tpm| is a non-owned pointer that must stay valid for the whole lifetime
  // of the created object. |delegate_blob| and |delegate_secret| should
  // correspond to a TPM delegate that allows doing signature-sealing
  // operations (currently used only on TPM 1.2).
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
  // |salt| is the vault keyset's randomly generated salt.
  // |key_challenge_callback| should be a callback that implements requesting
  // key challenges; in real use cases it will make IPC requests to the
  // service that talks to the cryptographic token with the challenged key.
  // The result is reported via |callback|.
  void GenerateNew(const std::string& account_id,
                   const ChallengePublicKeyInfo& public_key_info,
                   const brillo::Blob& salt,
                   const KeyChallengeCallback& key_challenge_callback,
                   const GenerateNewCallback& callback);

  // Builds credentials for the given user, based on the encrypted
  // (challenge-protected) representation of the previously created secrets
  // and the user's salt. The referred cryptographic key should be the same as
  // the one used for the secrets generation via GenerateNew(); although a
  // difference in the key's supported algorithms may be tolerated in some
  // cases. This operation involves making challenge request(s) against the
  // key.
  //
  // |public_key_info| describes the cryptographic key.
  // |salt| is the vault keyset's salt.
  // |keyset_challenge_info| is the encrypted representation of secrets as
  // created via GenerateNew().
  // |key_challenge_callback| should be a callback that implements requesting
  // key challenges; in real use cases it will make IPC requests to the
  // service that talks to the cryptographic token with the challenged key.
  // The result is reported via |callback|.
  void Decrypt(const std::string& account_id,
               const ChallengePublicKeyInfo& public_key_info,
               const brillo::Blob& salt,
               const KeysetSignatureChallengeInfo& keyset_challenge_info,
               const KeyChallengeCallback& key_challenge_callback,
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
  // |key_challenge_callback| should be a callback that implements requesting
  // key challenges; in real use cases it will make IPC requests to the
  // service that talks to the cryptographic token with the challenged key.
  // The result is reported via |callback|.
  void VerifyKey(const std::string& account_id,
                 const ChallengePublicKeyInfo& public_key_info,
                 const KeysetSignatureChallengeInfo& keyset_challenge_info,
                 const KeyChallengeCallback& key_challenge_callback,
                 const VerifyCallback& callback);

 private:
  // Non-owned.
  Tpm* const tpm_;
  // TPM delegate that was passed to the constructor.
  const brillo::Blob delegate_blob_;
  const brillo::Blob delegate_secret_;

  DISALLOW_COPY_AND_ASSIGN(ChallengeCredentialsHelper);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_HELPER_H_
