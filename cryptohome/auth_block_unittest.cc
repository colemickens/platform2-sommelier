// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/tpm_auth_block.h"

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cryptohome/crypto.h"
#include "cryptohome/crypto_error.h"
#include "cryptohome/mock_tpm.h"
#include "cryptohome/mock_tpm_init.h"
#include "cryptohome/vault_keyset.h"

using ::testing::_;
using ::testing::Exactly;
using ::testing::NiceMock;

namespace cryptohome {

TEST(TPMAuthBlockTest, DecryptBoundToPcrTest) {
  brillo::SecureBlob vault_key(20, 'C');
  brillo::SecureBlob tpm_key(20, 'B');
  brillo::SecureBlob salt(PKCS5_SALT_LEN, 'A');

  brillo::SecureBlob vkk_iv(kDefaultAesKeySize);
  brillo::SecureBlob vkk_key;

  brillo::SecureBlob pass_blob(kDefaultPassBlobSize);
  ASSERT_TRUE(CryptoLib::DeriveSecretsSCrypt(vault_key, salt, {&pass_blob}));

  NiceMock<MockTpm> tpm;
  NiceMock<MockTpmInit> tpm_init;
  EXPECT_CALL(tpm, UnsealWithAuthorization(_, _, pass_blob, _, _))
      .Times(Exactly(1));

  CryptoError error = CryptoError::CE_NONE;
  TpmAuthBlock tpm_auth_block(/*is_pcr_extended=*/false, &tpm, &tpm_init);
  EXPECT_TRUE(tpm_auth_block.DecryptTpmBoundToPcr(vault_key, tpm_key, salt,
                                                  &error, &vkk_iv, &vkk_key));
  EXPECT_EQ(CryptoError::CE_NONE, error);
}

TEST(TPMAuthBlockTest, DecryptNotBoundToPcrTest) {
  // Set up a SerializedVaultKeyset. In this case, it is only used to check the
  // flags and password_rounds.
  SerializedVaultKeyset serialized;
  serialized.set_flags(SerializedVaultKeyset::TPM_WRAPPED |
                       SerializedVaultKeyset::SCRYPT_DERIVED);

  brillo::SecureBlob vault_key(20, 'C');
  brillo::SecureBlob tpm_key(20, 'B');
  brillo::SecureBlob salt(PKCS5_SALT_LEN, 'A');

  brillo::SecureBlob vkk_key;
  brillo::SecureBlob vkk_iv(kDefaultAesKeySize);
  brillo::SecureBlob aes_key(kDefaultAesKeySize);

  ASSERT_TRUE(CryptoLib::DeriveSecretsSCrypt(vault_key, salt, {&aes_key}));

  NiceMock<MockTpm> tpm;
  NiceMock<MockTpmInit> tpm_init;
  EXPECT_CALL(tpm, DecryptBlob(_, tpm_key, aes_key, _, _)).Times(Exactly(1));

  CryptoError error = CryptoError::CE_NONE;
  TpmAuthBlock tpm_auth_block(/*is_pcr_extended=*/false, &tpm, &tpm_init);
  EXPECT_TRUE(tpm_auth_block.DecryptTpmNotBoundToPcr(
      serialized, vault_key, tpm_key, salt, &error, &vkk_iv, &vkk_key));
  EXPECT_EQ(CryptoError::CE_NONE, error);
}

TEST(TpmAuthBlockTest, DeriveTest) {
  SerializedVaultKeyset serialized;
  serialized.set_flags(SerializedVaultKeyset::TPM_WRAPPED |
                       SerializedVaultKeyset::PCR_BOUND |
                       SerializedVaultKeyset::SCRYPT_DERIVED);

  brillo::SecureBlob key(20, 'B');
  brillo::SecureBlob tpm_key(20, 'C');
  std::string salt(PKCS5_SALT_LEN, 'A');

  serialized.set_salt(salt);
  serialized.set_tpm_key(tpm_key.data(), tpm_key.size());

  // Make sure TpmAuthBlock calls DecryptTpmBoundToPcr in this case.
  NiceMock<MockTpm> tpm;
  NiceMock<MockTpmInit> tpm_init;
  EXPECT_CALL(tpm, UnsealWithAuthorization(_, _, _, _, _)).Times(Exactly(1));

  TpmAuthBlock auth_block(/*is_pcr_extended=*/false, &tpm, &tpm_init);

  KeyBlobs key_out_data;
  AuthInput user_input = {key};
  AuthBlockState auth_state = {
      base::make_optional<SerializedVaultKeyset>(std::move(serialized))};
  CryptoError error;
  EXPECT_TRUE(auth_block.Derive(user_input, auth_state, &key_out_data, &error));

  // Assert that the returned key blobs isn't uninitialized.
  EXPECT_NE(key_out_data.vkk_iv, base::nullopt);
  EXPECT_NE(key_out_data.vkk_key, base::nullopt);
  EXPECT_EQ(key_out_data.vkk_iv.value(), key_out_data.chaps_iv.value());
  EXPECT_EQ(key_out_data.vkk_iv.value(),
            key_out_data.authorization_data_iv.value());
}

}  // namespace cryptohome
