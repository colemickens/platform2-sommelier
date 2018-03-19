// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/cryptolib.h"

#include <openssl/rsa.h>

#include <brillo/secure_blob.h>
#include <crypto/scoped_openssl_types.h>
#include <gtest/gtest.h>

using brillo::SecureBlob;

namespace cryptohome {

TEST(CryptoLibTest, RsaOaepDecrypt) {
  // Generate the input data.
  constexpr int kKeySizeBits = 1024;
  constexpr int kKeySizeBytes = kKeySizeBits / 8;
  constexpr int kPlaintextSize = 32;
  crypto::ScopedRSA rsa(
      RSA_generate_key(kKeySizeBits, kWellKnownExponent, nullptr, nullptr));
  ASSERT_TRUE(rsa);
  SecureBlob plaintext(kPlaintextSize);
  CryptoLib::GetSecureRandom(plaintext.data(), plaintext.size());
  // Test decryption when a non-empty label is used.
  const SecureBlob kFirstOaepLabel("foo");
  SecureBlob first_padded_data(kKeySizeBytes);
  ASSERT_EQ(
      1, RSA_padding_add_PKCS1_OAEP(
             first_padded_data.data(), kKeySizeBytes, plaintext.data(),
             plaintext.size(), kFirstOaepLabel.data(), kFirstOaepLabel.size()));
  SecureBlob first_ciphertext(kKeySizeBytes);
  ASSERT_NE(-1, RSA_public_encrypt(kKeySizeBytes, first_padded_data.data(),
                                   first_ciphertext.data(), rsa.get(),
                                   RSA_NO_PADDING));
  SecureBlob first_decrypt_result;
  EXPECT_TRUE(CryptoLib::RsaOaepDecrypt(first_ciphertext, kFirstOaepLabel,
                                        rsa.get(), &first_decrypt_result));
  EXPECT_EQ(plaintext, first_decrypt_result);
  // Test the empty label case in which the encryption is done by a single call
  // to OpenSSL.
  SecureBlob second_ciphertext(kKeySizeBytes);
  ASSERT_NE(-1, RSA_public_encrypt(kPlaintextSize, plaintext.data(),
                                   second_ciphertext.data(), rsa.get(),
                                   RSA_PKCS1_OAEP_PADDING));
  SecureBlob second_decrypt_result;
  EXPECT_TRUE(CryptoLib::RsaOaepDecrypt(second_ciphertext, SecureBlob(),
                                        rsa.get(), &second_decrypt_result));
  EXPECT_EQ(plaintext, second_decrypt_result);
}

}  // namespace cryptohome
