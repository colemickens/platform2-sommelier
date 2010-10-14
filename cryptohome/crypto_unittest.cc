// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
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

#include "mock_tpm.h"

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
      if (memcmp(&haystack[start], &needle[0], needle.size()) == 0) {
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

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptoTest);
};

TEST_F(CryptoTest, RandomTest) {
  // Check that GetSecureRandom() returns different bytes than are passed in or
  // that come from the entropy source
  Crypto crypto;
  crypto.set_entropy_source("/dev/zero");

  unsigned char data[32];
  memset(data, 1, sizeof(data));

  crypto.GetSecureRandom(data, sizeof(data));

  unsigned char comparison[32];
  memset(comparison, 0, sizeof(comparison));
  EXPECT_NE(0, memcmp(data, comparison, sizeof(data)));

  memset(comparison, 1, sizeof(comparison));
  EXPECT_NE(0, memcmp(data, comparison, sizeof(data)));
}

TEST_F(CryptoTest, EncryptionTest) {
  // Check that WrapVaultKeyset returns something other than the bytes passed
  Crypto crypto;

  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(crypto);

  SecureBlob wrapper(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(wrapper.data()),
                         wrapper.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.WrapVaultKeyset(vault_keyset, wrapper, salt, &serialized));

  SecureBlob original;
  ASSERT_TRUE(vault_keyset.ToKeysBlob(&original));
  SecureBlob wrapped;
  GetSerializedBlob(serialized, &wrapped);

  ASSERT_TRUE(wrapped.size() > 0);
  ASSERT_FALSE(CryptoTest::FindBlobInBlob(wrapped, original));
}

TEST_F(CryptoTest, DecryptionTest) {
  // Check that UnwrapVaultKeyset returns the original keyset
  Crypto crypto;

  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(crypto);

  SecureBlob wrapper(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(wrapper.data()),
                         wrapper.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.WrapVaultKeyset(vault_keyset, wrapper, salt, &serialized));
  SecureBlob wrapped;
  GetSerializedBlob(serialized, &wrapped);

  ASSERT_TRUE(CryptoTest::FindBlobInBlob(wrapped, salt));

  ASSERT_TRUE(CryptoTest::FromSerializedBlob(wrapped, &serialized));

  VaultKeyset new_keyset;
  unsigned int wrap_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  ASSERT_TRUE(crypto.UnwrapVaultKeyset(serialized, wrapper, &wrap_flags,
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
  Crypto crypto;
  NiceMock<MockTpm> tpm;

  crypto.set_tpm(&tpm);
  crypto.set_use_tpm(true);

  EXPECT_CALL(tpm, Init(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(tpm, Encrypt(_, _, _, _, _, _));
  EXPECT_CALL(tpm, Decrypt(_, _, _, _, _, _));
  EXPECT_CALL(tpm, IsConnected())
      .WillRepeatedly(Return(true));

  crypto.Init();

  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(crypto);

  SecureBlob wrapper(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(wrapper.data()),
                         wrapper.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.WrapVaultKeyset(vault_keyset, wrapper, salt, &serialized));
  SecureBlob wrapped;
  GetSerializedBlob(serialized, &wrapped);

  ASSERT_TRUE(CryptoTest::FindBlobInBlob(wrapped, salt));

  ASSERT_TRUE(CryptoTest::FromSerializedBlob(wrapped, &serialized));

  VaultKeyset new_keyset;
  unsigned int wrap_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  ASSERT_TRUE(crypto.UnwrapVaultKeyset(serialized, wrapper, &wrap_flags,
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
  Crypto crypto;

  crypto.set_fallback_to_scrypt(true);

  crypto.Init();

  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(crypto);

  SecureBlob wrapper(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(wrapper.data()),
                         wrapper.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.WrapVaultKeyset(vault_keyset, wrapper, salt, &serialized));
  SecureBlob wrapped;
  GetSerializedBlob(serialized, &wrapped);

  ASSERT_TRUE(CryptoTest::FindBlobInBlob(wrapped, salt));

  ASSERT_TRUE(CryptoTest::FromSerializedBlob(wrapped, &serialized));

  VaultKeyset new_keyset;
  unsigned int wrap_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  ASSERT_TRUE(crypto.UnwrapVaultKeyset(serialized, wrapper, &wrap_flags,
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
  Crypto crypto;
  NiceMock<MockTpm> tpm;

  crypto.set_tpm(&tpm);
  crypto.set_use_tpm(true);
  crypto.set_fallback_to_scrypt(true);

  EXPECT_CALL(tpm, Init(_, _)).WillOnce(Return(true));
  EXPECT_CALL(tpm, Encrypt(_, _, _, _, _, _));
  EXPECT_CALL(tpm, Decrypt(_, _, _, _, _, _));

  crypto.Init();

  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(crypto);

  SecureBlob wrapper(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(wrapper.data()),
                         wrapper.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SerializedVaultKeyset serialized;
  ASSERT_TRUE(crypto.WrapVaultKeyset(vault_keyset, wrapper, salt, &serialized));
  SecureBlob wrapped;
  GetSerializedBlob(serialized, &wrapped);

  ASSERT_TRUE(CryptoTest::FindBlobInBlob(wrapped, salt));

  ASSERT_TRUE(CryptoTest::FromSerializedBlob(wrapped, &serialized));

  VaultKeyset new_keyset;
  unsigned int wrap_flags = 0;
  Crypto::CryptoError crypto_error = Crypto::CE_NONE;
  ASSERT_TRUE(crypto.UnwrapVaultKeyset(serialized, wrapper, &wrap_flags,
                                       &crypto_error, &new_keyset));

  SecureBlob original_data;
  ASSERT_TRUE(vault_keyset.ToKeysBlob(&original_data));
  SecureBlob new_data;
  ASSERT_TRUE(new_keyset.ToKeysBlob(&new_data));

  EXPECT_EQ(new_data.size(), original_data.size());
  ASSERT_TRUE(CryptoTest::FindBlobInBlob(new_data, original_data));
}

} // namespace cryptohome
