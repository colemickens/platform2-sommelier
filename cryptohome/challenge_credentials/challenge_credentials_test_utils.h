// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_TEST_UTILS_H_
#define CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_TEST_UTILS_H_

#include <memory>
#include <string>

#include <brillo/secure_blob.h>

#include "cryptohome/challenge_credentials/challenge_credentials_decrypt_operation.h"

#include "key.pb.h"           // NOLINT(build/include)
#include "rpc.pb.h"           // NOLINT(build/include)
#include "vault_keyset.pb.h"  // NOLINT(build/include)

namespace cryptohome {

class UsernamePasskey;

// Holds result returned from ChallengeCredentialsDecryptOperation.
struct ChallengeCredentialsDecryptResult {
  std::unique_ptr<UsernamePasskey> username_passkey;
};

// Returns a callback for ChallengeCredentialsDecryptOperation that stores the
// result into the given smart pointer. The smart pointer will become non-null
// after the callback gets executed.
ChallengeCredentialsDecryptOperation::CompletionCallback
MakeChallengeCredentialsDecryptResultWriter(
    std::unique_ptr<ChallengeCredentialsDecryptResult>* result);

// Verifies that the ChallengeCredentialsDecryptOperation result is a
// valid success result.
void VerifySuccessfulChallengeCredentialsDecryptResult(
    const ChallengeCredentialsDecryptResult& result,
    const std::string& expected_username,
    const brillo::SecureBlob& expected_passkey);

// Verifies that the ChallengeCredentialsDecryptOperation result is a
// failure result.
void VerifyFailedChallengeCredentialsDecryptResult(
    const ChallengeCredentialsDecryptResult& result);

}  // namespace cryptohome

#endif  // CRYPTOHOME_CHALLENGE_CREDENTIALS_CHALLENGE_CREDENTIALS_TEST_UTILS_H_
