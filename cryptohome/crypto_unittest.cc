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
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>
#include <vector>

#include "mock_platform.h"
#include "mock_tpm.h"

namespace cryptohome {
using chromeos::SecureBlob;
using std::string;
using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;

const char kImageDir[] = "test_image_dir";

// FIPS 180-2 test vectors for SHA-1 and SHA-256
class ShaTestVectors {
 public:
  explicit ShaTestVectors(int type);

  ~ShaTestVectors() { }
  const chromeos::Blob* input(int index) const { return &input_[index]; }
  const chromeos::SecureBlob* output(int index) const { return &output_[index];}
  size_t count() const { return 3; }  // sizeof(input_); }

  static const char* kOneBlockMessage;
  static const char* kMultiBlockMessage;
  static const uint8_t kSha1Results[][SHA_DIGEST_LENGTH];
  static const uint8_t kSha256Results[][SHA256_DIGEST_LENGTH];
 private:
  chromeos::Blob input_[3];
  chromeos::SecureBlob output_[3];
};

const char* ShaTestVectors::kMultiBlockMessage =
    "abcdbcdecdefdefgefghfghighijhijkijkl"
    "jklmklmnlmnomnopnopq";
const char* ShaTestVectors::kOneBlockMessage = "abc";
const uint8_t ShaTestVectors::kSha1Results[][SHA_DIGEST_LENGTH] = {
  {
    0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a,
    0xba, 0x3e, 0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c,
    0x9c, 0xd0, 0xd8, 0x9d
  }, {
    0x84, 0x98, 0x3e, 0x44, 0x1c, 0x3b, 0xd2, 0x6e,
    0xba, 0xae, 0x4a, 0xa1, 0xf9, 0x51, 0x29, 0xe5,
    0xe5, 0x46, 0x70, 0xf1
  }, {
    0x34, 0xaa, 0x97, 0x3c, 0xd4, 0xc4, 0xda, 0xa4,
    0xf6, 0x1e, 0xeb, 0x2b, 0xdb, 0xad, 0x27, 0x31,
    0x65, 0x34, 0x01, 0x6f
  }
};
const uint8_t ShaTestVectors::kSha256Results[][SHA256_DIGEST_LENGTH] = {
  {
    0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
    0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
    0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
    0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
  }, {
    0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
    0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
    0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
    0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
  }, {
    0xcd, 0xc7, 0x6e, 0x5c, 0x99, 0x14, 0xfb, 0x92,
    0x81, 0xa1, 0xc7, 0xe2, 0x84, 0xd7, 0x3e, 0x67,
    0xf1, 0x80, 0x9a, 0x48, 0xa4, 0x97, 0x20, 0x0e,
    0x04, 0x6d, 0x39, 0xcc, 0xc7, 0x11, 0x2c, 0xd0
  }
};

ShaTestVectors::ShaTestVectors(int type) {
  // Since we don't do 512+, we can prep here for all types and
  // don't need to get fancy.
  input_[0].resize(strlen(kOneBlockMessage));
  memcpy(&(input_[0][0]), kOneBlockMessage, input_[0].size());
  input_[1].resize(strlen(kMultiBlockMessage));
  memcpy(&input_[1][0], kMultiBlockMessage, input_[1].size());
  input_[2].assign(1000000, 'a');

  switch (type) {
    case 1:
      for (size_t i = 0; i < count(); ++i) {
        output_[i].resize(SHA_DIGEST_LENGTH);
        memcpy(output_[i].data(), kSha1Results[i], output_[i].size());
      }
      break;
    case 256:
      for (size_t i = 0; i < count(); ++i) {
        output_[i].resize(SHA256_DIGEST_LENGTH);
        memcpy(output_[i].data(), kSha256Results[i], output_[i].size());
      }
      break;
    default:
      CHECK(false) << "Only SHA-256 and SHA-1 are supported";
  }
}

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
  for (size_t i = 0; i < vectors.count(); ++i) {
    SecureBlob digest = crypto.GetSha1(*vectors.input(i));
    std::string computed(reinterpret_cast<const char*>(digest.data()),
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
  for (size_t i = 0; i < vectors.count(); ++i) {
    SecureBlob digest = crypto.GetSha256(*vectors.input(i));
    std::string computed(reinterpret_cast<const char*>(digest.data()),
                         digest.size());
    std::string expected(
      reinterpret_cast<const char*>(vectors.output(i)->const_data()),
                                    vectors.output(i)->size());
    EXPECT_EQ(expected, computed);
  }
}

}  // namespace cryptohome
