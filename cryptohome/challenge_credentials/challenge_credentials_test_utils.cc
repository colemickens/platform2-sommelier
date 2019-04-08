// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/challenge_credentials/challenge_credentials_test_utils.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cryptohome/credentials.h"

using brillo::Blob;
using brillo::SecureBlob;

namespace cryptohome {

ChallengeCredentialsHelper::GenerateNewCallback
MakeChallengeCredentialsGenerateNewResultWriter(
    std::unique_ptr<ChallengeCredentialsGenerateNewResult>* result) {
  DCHECK(!*result);
  return base::Bind(
      [](std::unique_ptr<ChallengeCredentialsGenerateNewResult>* result,
         std::unique_ptr<Credentials> username_passkey) {
        ASSERT_FALSE(*result);
        *result = std::make_unique<ChallengeCredentialsGenerateNewResult>();
        (*result)->username_passkey = std::move(username_passkey);
      },
      base::Unretained(result));
}

ChallengeCredentialsHelper::DecryptCallback
MakeChallengeCredentialsDecryptResultWriter(
    std::unique_ptr<ChallengeCredentialsDecryptResult>* result) {
  DCHECK(!*result);
  return base::Bind(
      [](std::unique_ptr<ChallengeCredentialsDecryptResult>* result,
         std::unique_ptr<Credentials> username_passkey) {
        ASSERT_FALSE(*result);
        *result = std::make_unique<ChallengeCredentialsDecryptResult>();
        (*result)->username_passkey = std::move(username_passkey);
      },
      base::Unretained(result));
}

void VerifySuccessfulChallengeCredentialsGenerateNewResult(
    const ChallengeCredentialsGenerateNewResult& result,
    const std::string& expected_username,
    const SecureBlob& expected_passkey) {
  ASSERT_TRUE(result.username_passkey);
  EXPECT_EQ(expected_username, result.username_passkey->username());
  SecureBlob passkey;
  result.username_passkey->GetPasskey(&passkey);
  EXPECT_EQ(expected_passkey, passkey);
  EXPECT_EQ(KeyData::KEY_TYPE_CHALLENGE_RESPONSE,
            result.username_passkey->key_data().type());
}

void VerifySuccessfulChallengeCredentialsDecryptResult(
    const ChallengeCredentialsDecryptResult& result,
    const std::string& expected_username,
    const SecureBlob& expected_passkey) {
  ASSERT_TRUE(result.username_passkey);
  EXPECT_EQ(expected_username, result.username_passkey->username());
  SecureBlob passkey;
  result.username_passkey->GetPasskey(&passkey);
  EXPECT_EQ(expected_passkey, passkey);
  EXPECT_EQ(KeyData::KEY_TYPE_CHALLENGE_RESPONSE,
            result.username_passkey->key_data().type());
}

void VerifyFailedChallengeCredentialsGenerateNewResult(
    const ChallengeCredentialsGenerateNewResult& result) {
  EXPECT_FALSE(result.username_passkey);
}

void VerifyFailedChallengeCredentialsDecryptResult(
    const ChallengeCredentialsDecryptResult& result) {
  EXPECT_FALSE(result.username_passkey);
}

}  // namespace cryptohome
