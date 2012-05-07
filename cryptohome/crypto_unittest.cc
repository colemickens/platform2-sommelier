// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Unit tests for Crypto.

#include "crypto.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>
#include <vector>

#include "mock_platform.h"
#include "mock_tpm.h"
#include "sha_test_vectors.h"

namespace cryptohome {
using std::string;
using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;

const char kImageDir[] = "test_image_dir";

class CryptoTest : public ::testing::Test {
 public:
  CryptoTest() { }
  virtual ~CryptoTest() { }

  static bool FindBlobInBlob(const SecureBlob& haystack,
                             const SecureBlob& needle) {
    if (needle.size() > haystack.size()) {
      return false;
    }
    for (unsigned int start = 0; start <= (haystack.size() - needle.size());
         start++) {
      if (chromeos::SafeMemcmp(&haystack[start], &needle[0],
                               needle.size()) == 0) {
        return true;
      }
    }
    return false;
  }

  static void GetSerializedBlob(const SerializedVaultKeyset& serialized,
                                SecureBlob* blob) {
    SecureBlob final_blob(serialized.ByteSize());
    serialized.SerializeWithCachedSizesToArray(
        static_cast<google::protobuf::uint8*>(final_blob.data()));
    blob->swap(final_blob);
  }

  static bool FromSerializedBlob(const SecureBlob& blob,
                                 SerializedVaultKeyset* serialized) {
    return serialized->ParseFromArray(
        static_cast<const unsigned char*>(blob.const_data()),
        blob.size());
  }

 protected:
  MockPlatform platform_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptoTest);
};

TEST_F(CryptoTest, EncryptionTest) {
  // Check that EncryptVaultKeyset returns something other than the bytes passed
  Crypto crypto;

  VaultKeyset vault_keyset(&platform_, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob key(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(key.data()), key.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.EncryptVaultKeyset(vault_keyset, key, salt, &serialized));

  SecureBlob original;
  ASSERT_TRUE(vault_keyset.ToKeysBlob(&original));
  SecureBlob encrypted;
  GetSerializedBlob(serialized, &encrypted);

  ASSERT_GT(encrypted.size(), 0);
  ASSERT_FALSE(CryptoTest::FindBlobInBlob(encrypted, original));
}

TEST_F(CryptoTest, DecryptionTest) {
  // Check that DecryptVaultKeyset returns the original keyset
  Crypto crypto;

  VaultKeyset vault_keyset(&platform_, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob key(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(key.data()), key.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.EncryptVaultKeyset(vault_keyset, key, salt, &serialized));
  SecureBlob encrypted;
  GetSerializedBlob(serialized, &encrypted);

  ASSERT_TRUE(CryptoTest::FindBlobInBlob(encrypted, salt));

  ASSERT_TRUE(CryptoTest::FromSerializedBlob(encrypted, &serialized));

  VaultKeyset new_keyset(&platform_, &crypto);
  unsigned int crypt_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  ASSERT_TRUE(crypto.DecryptVaultKeyset(serialized, key, &crypt_flags,
                                        &crypto_error, &new_keyset));

  SecureBlob original_data;
  ASSERT_TRUE(vault_keyset.ToKeysBlob(&original_data));
  SecureBlob new_data;
  ASSERT_TRUE(new_keyset.ToKeysBlob(&new_data));

  EXPECT_EQ(new_data.size(), original_data.size());
  ASSERT_TRUE(CryptoTest::FindBlobInBlob(new_data, original_data));
}

TEST_F(CryptoTest, SaltCreateTest) {
  // Check that GetOrCreateSalt works
  Crypto crypto;

  FilePath salt_path(FilePath(kImageDir).Append("crypto_test_salt"));

  file_util::Delete(salt_path, false);

  ASSERT_FALSE(file_util::PathExists(salt_path));

  SecureBlob salt;
  crypto.GetOrCreateSalt(salt_path, 32, false, &salt);

  ASSERT_EQ(32, salt.size());
  ASSERT_TRUE(file_util::PathExists(salt_path));

  SecureBlob new_salt;
  crypto.GetOrCreateSalt(salt_path, 32, true, &new_salt);

  ASSERT_EQ(32, new_salt.size());
  ASSERT_TRUE(file_util::PathExists(salt_path));

  ASSERT_EQ(salt.size(), new_salt.size());
  ASSERT_FALSE(CryptoTest::FindBlobInBlob(salt, new_salt));

  file_util::Delete(salt_path, false);
}

TEST_F(CryptoTest, AsciiEncodeTest) {
  // Check that AsciiEncodeToBuffer works
  Crypto crypto;

  SecureBlob blob_in(256);
  SecureBlob blob_out(512);

  for (int i = 0; i < 256; i++) {
    blob_in[i] = i;
    blob_out[i * 2] = 0;
    blob_out[i * 2 + 1] = 0;
  }

  crypto.AsciiEncodeToBuffer(blob_in, static_cast<char*>(blob_out.data()),
                             blob_out.size());

  std::string known_good = chromeos::AsciiEncode(blob_in);
  std::string test_good(static_cast<char*>(blob_out.data()), blob_out.size());

  ASSERT_EQ(0, known_good.compare(test_good));
}

TEST_F(CryptoTest, TpmStepTest) {
  // Check that the code path changes to support the TPM work
  MockPlatform platform;
  Crypto crypto;
  NiceMock<MockTpm> tpm;

  crypto.set_tpm(&tpm);
  crypto.set_use_tpm(true);

  EXPECT_CALL(tpm, Init(_, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(tpm, Encrypt(_, _, _, _, _, _));
  EXPECT_CALL(tpm, Decrypt(_, _, _, _, _, _));
  EXPECT_CALL(tpm, IsConnected())
      .WillRepeatedly(Return(true));

  crypto.Init(&platform);

  VaultKeyset vault_keyset(&platform_, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob key(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(key.data()), key.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.EncryptVaultKeyset(vault_keyset, key, salt, &serialized));
  SecureBlob encrypted;
  GetSerializedBlob(serialized, &encrypted);

  ASSERT_TRUE(CryptoTest::FindBlobInBlob(encrypted, salt));

  ASSERT_TRUE(CryptoTest::FromSerializedBlob(encrypted, &serialized));

  VaultKeyset new_keyset(&platform, &crypto);
  unsigned int crypt_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  ASSERT_TRUE(crypto.DecryptVaultKeyset(serialized, key, &crypt_flags,
                                        &crypto_error, &new_keyset));

  SecureBlob original_data;
  ASSERT_TRUE(vault_keyset.ToKeysBlob(&original_data));
  SecureBlob new_data;
  ASSERT_TRUE(new_keyset.ToKeysBlob(&new_data));

  EXPECT_EQ(new_data.size(), original_data.size());
  ASSERT_TRUE(CryptoTest::FindBlobInBlob(new_data, original_data));
}

TEST_F(CryptoTest, ScryptStepTest) {
  // Check that the code path changes to support scrypt work
  MockPlatform platform;
  Crypto crypto;

  crypto.Init(&platform);

  VaultKeyset vault_keyset(&platform, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob key(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(key.data()), key.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.EncryptVaultKeyset(vault_keyset, key, salt, &serialized));
  SecureBlob encrypted;
  GetSerializedBlob(serialized, &encrypted);

  ASSERT_TRUE(CryptoTest::FindBlobInBlob(encrypted, salt));

  ASSERT_TRUE(CryptoTest::FromSerializedBlob(encrypted, &serialized));

  VaultKeyset new_keyset(&platform, &crypto);
  unsigned int crypt_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  ASSERT_TRUE(crypto.DecryptVaultKeyset(serialized, key, &crypt_flags,
                                        &crypto_error, &new_keyset));

  SecureBlob original_data;
  ASSERT_TRUE(vault_keyset.ToKeysBlob(&original_data));
  SecureBlob new_data;
  ASSERT_TRUE(new_keyset.ToKeysBlob(&new_data));

  EXPECT_EQ(new_data.size(), original_data.size());
  ASSERT_TRUE(CryptoTest::FindBlobInBlob(new_data, original_data));
}

TEST_F(CryptoTest, TpmScryptStepTest) {
  // Check that the code path changes to support when TPM + scrypt fallback are
  // enabled
  MockPlatform platform;
  Crypto crypto;
  NiceMock<MockTpm> tpm;

  crypto.set_tpm(&tpm);
  crypto.set_use_tpm(true);

  EXPECT_CALL(tpm, Init(_, _, _)).WillOnce(Return(true));
  EXPECT_CALL(tpm, Encrypt(_, _, _, _, _, _));
  EXPECT_CALL(tpm, Decrypt(_, _, _, _, _, _));

  crypto.Init(&platform);

  VaultKeyset vault_keyset(&platform_, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob key(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(key.data()), key.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.EncryptVaultKeyset(vault_keyset, key, salt, &serialized));
  SecureBlob encrypted;
  GetSerializedBlob(serialized, &encrypted);

  ASSERT_TRUE(CryptoTest::FindBlobInBlob(encrypted, salt));

  ASSERT_TRUE(CryptoTest::FromSerializedBlob(encrypted, &serialized));

  VaultKeyset new_keyset(&platform_, &crypto);
  unsigned int crypt_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  ASSERT_TRUE(crypto.DecryptVaultKeyset(serialized, key, &crypt_flags,
                                        &crypto_error, &new_keyset));

  SecureBlob original_data;
  ASSERT_TRUE(vault_keyset.ToKeysBlob(&original_data));
  SecureBlob new_data;
  ASSERT_TRUE(new_keyset.ToKeysBlob(&new_data));

  EXPECT_EQ(new_data.size(), original_data.size());
  ASSERT_TRUE(CryptoTest::FindBlobInBlob(new_data, original_data));
}

TEST_F(CryptoTest, GetSha1FipsTest) {
  Crypto crypto;
  ShaTestVectors vectors(1);
  SecureBlob digest;
  for (size_t i = 0; i < vectors.count(); ++i) {
    crypto.GetSha1(*vectors.input(i), 0, vectors.input(i)->size(), &digest);
    std::string computed(reinterpret_cast<const char*>(digest.const_data()),
                         digest.size());
    std::string expected(
      reinterpret_cast<const char*>(vectors.output(i)->const_data()),
                                    vectors.output(i)->size());
    EXPECT_EQ(expected, computed);
  }
}

TEST_F(CryptoTest, GetSha256FipsTest) {
  Crypto crypto;
  ShaTestVectors vectors(256);
  SecureBlob digest;
  for (size_t i = 0; i < vectors.count(); ++i) {
    crypto.GetSha256(*vectors.input(i), 0, vectors.input(i)->size(), &digest);
    std::string computed(reinterpret_cast<const char*>(digest.const_data()),
                         digest.size());
    std::string expected(
      reinterpret_cast<const char*>(vectors.output(i)->const_data()),
                                    vectors.output(i)->size());
    EXPECT_EQ(expected, computed);
  }
}

}  // namespace cryptohome
