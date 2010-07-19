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

  SecureBlob wrapped;
  EXPECT_EQ(true, crypto.WrapVaultKeyset(vault_keyset, wrapper, salt,
                                         &wrapped));

  SecureBlob original;
  EXPECT_EQ(true, vault_keyset.ToKeysBlob(&original));

  EXPECT_EQ(false, CryptoTest::FindBlobInBlob(wrapped, original));
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

  SecureBlob wrapped;
  EXPECT_EQ(true, crypto.WrapVaultKeyset(vault_keyset, wrapper, salt,
                                         &wrapped));

  EXPECT_EQ(true, CryptoTest::FindBlobInBlob(wrapped, salt));

  VaultKeyset new_keyset;
  bool tpm_wrapped = false;
  EXPECT_EQ(true, crypto.UnwrapVaultKeyset(wrapped, wrapper, &tpm_wrapped,
                                           &new_keyset));

  SecureBlob original_data;
  EXPECT_EQ(true, vault_keyset.ToKeysBlob(&original_data));
  SecureBlob new_data;
  EXPECT_EQ(true, new_keyset.ToKeysBlob(&new_data));

  EXPECT_EQ(new_data.size(), original_data.size());
  EXPECT_EQ(true, CryptoTest::FindBlobInBlob(new_data, original_data));
}

TEST_F(CryptoTest, SaltCreateTest) {
  // Check that GetOrCreateSalt works
  Crypto crypto;

  FilePath salt_path(FilePath(kImageDir).Append("crypto_test_salt"));

  file_util::Delete(salt_path, false);

  EXPECT_EQ(false, file_util::PathExists(salt_path));

  SecureBlob salt;
  crypto.GetOrCreateSalt(salt_path, 32, false, &salt);

  EXPECT_EQ(32, salt.size());
  EXPECT_EQ(true, file_util::PathExists(salt_path));

  SecureBlob new_salt;
  crypto.GetOrCreateSalt(salt_path, 32, true, &new_salt);

  EXPECT_EQ(32, new_salt.size());
  EXPECT_EQ(true, file_util::PathExists(salt_path));

  EXPECT_EQ(salt.size(), new_salt.size());
  EXPECT_EQ(false, CryptoTest::FindBlobInBlob(salt, new_salt));

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

  EXPECT_EQ(0, known_good.compare(test_good));
}

TEST_F(CryptoTest, TpmStepTest) {
  // Check that the code path changes to support the TPM work
  Crypto crypto;
  MockTpm tpm;

  crypto.set_tpm(&tpm);
  crypto.set_use_tpm(true);

  EXPECT_CALL(tpm, Init(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(tpm, Encrypt(_, _, _, _));
  EXPECT_CALL(tpm, Decrypt(_, _, _, _));

  crypto.Init();

  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(crypto);

  SecureBlob wrapper(20);
  crypto.GetSecureRandom(static_cast<unsigned char*>(wrapper.data()),
                         wrapper.size());
  SecureBlob salt(PKCS5_SALT_LEN);
  crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                         salt.size());

  SecureBlob wrapped;
  EXPECT_EQ(true, crypto.WrapVaultKeyset(vault_keyset, wrapper, salt,
                                         &wrapped));

  EXPECT_EQ(true, CryptoTest::FindBlobInBlob(wrapped, salt));

  VaultKeyset new_keyset;
  bool tpm_wrapped = false;
  EXPECT_EQ(true, crypto.UnwrapVaultKeyset(wrapped, wrapper, &tpm_wrapped,
                                           &new_keyset));

  SecureBlob original_data;
  EXPECT_EQ(true, vault_keyset.ToKeysBlob(&original_data));
  SecureBlob new_data;
  EXPECT_EQ(true, new_keyset.ToKeysBlob(&new_data));

  EXPECT_EQ(new_data.size(), original_data.size());
  EXPECT_EQ(true, CryptoTest::FindBlobInBlob(new_data, original_data));
}

} // namespace cryptohome
