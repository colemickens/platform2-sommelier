// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for VaultKeyset.

#include "cryptohome/vault_keyset.h"

#include <base/logging.h>
#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cryptohome/crypto.h"
#include "cryptohome/cryptohome_common.h"
#include "cryptohome/mock_platform.h"

namespace cryptohome {
using chromeos::SecureBlob;
using std::string;

using ::testing::_;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::Return;
using ::testing::WithArg;

class VaultKeysetTest : public ::testing::Test {
 public:
  VaultKeysetTest() { }
  virtual ~VaultKeysetTest() { }

  static bool FindBlobInBlob(const chromeos::Blob& haystack,
                             const chromeos::Blob& needle) {
    if (needle.size() > haystack.size()) {
      return false;
    }
    for (unsigned int start = 0; start <= (haystack.size() - needle.size());
         start++) {
      if (chromeos::SecureMemcmp(&haystack[start], &needle[0],
                                 needle.size()) == 0) {
        return true;
      }
    }
    return false;
  }

 protected:
  MockPlatform platform_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VaultKeysetTest);
};

TEST_F(VaultKeysetTest, AllocateRandom) {
  // Check that allocating a random VaultKeyset works
  Crypto crypto(&platform_);
  VaultKeyset vault_keyset;
  vault_keyset.Initialize(&platform_, &crypto);
  vault_keyset.CreateRandom();

  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SIZE, vault_keyset.fek().size());
  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE,
            vault_keyset.fek_sig().size());
  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE, vault_keyset.fek_salt().size());

  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SIZE, vault_keyset.fnek().size());
  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE,
            vault_keyset.fnek_sig().size());
  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE, vault_keyset.fnek_salt().size());
  EXPECT_EQ(CRYPTOHOME_CHAPS_KEY_LENGTH, vault_keyset.chaps_key().size());
}

TEST_F(VaultKeysetTest, SerializeTest) {
  // Check that serialize works
  Crypto crypto(&platform_);
  VaultKeyset vault_keyset;
  vault_keyset.Initialize(&platform_, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob blob;
  EXPECT_TRUE(vault_keyset.ToKeysBlob(&blob));

  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.fek()));
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.fek_sig()));
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.fek_salt()));

  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.fnek()));
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.fnek_sig()));
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.fnek_salt()));
}

TEST_F(VaultKeysetTest, DeserializeTest) {
  // Check that deserialize works
  Crypto crypto(&platform_);
  VaultKeyset vault_keyset;
  vault_keyset.Initialize(&platform_, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob blob;
  EXPECT_TRUE(vault_keyset.ToKeysBlob(&blob));

  VaultKeyset new_vault_keyset;
  new_vault_keyset.FromKeysBlob(blob);

  EXPECT_EQ(vault_keyset.fek().size(), new_vault_keyset.fek().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.fek(),
                                              new_vault_keyset.fek()));
  EXPECT_EQ(vault_keyset.fek_sig().size(), new_vault_keyset.fek_sig().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.fek_sig(),
                                              new_vault_keyset.fek_sig()));
  EXPECT_EQ(vault_keyset.fek_salt().size(), new_vault_keyset.fek_salt().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.fek_salt(),
                                              new_vault_keyset.fek_salt()));

  EXPECT_EQ(vault_keyset.fnek().size(), new_vault_keyset.fnek().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.fnek(),
                                              new_vault_keyset.fnek()));
  EXPECT_EQ(vault_keyset.fnek_sig().size(), new_vault_keyset.fnek_sig().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.fnek_sig(),
                                              new_vault_keyset.fnek_sig()));
  EXPECT_EQ(vault_keyset.fnek_salt().size(),
            new_vault_keyset.fnek_salt().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.fnek_salt(),
                                              new_vault_keyset.fnek_salt()));
}

ACTION_P(CopyToSecureBlob, b) {
  b->assign(arg0.begin(), arg0.end());
  return true;
}

ACTION_P(CopyFromSecureBlob, b) {
  arg0->assign(b->begin(), b->end());
  return true;
}

TEST_F(VaultKeysetTest, LoadSaveTest) {
  MockPlatform platform;
  Crypto crypto(&platform);
  VaultKeyset keyset;
  keyset.Initialize(&platform, &crypto);

  keyset.CreateRandom();
  SecureBlob bytes;

  EXPECT_CALL(platform, WriteFile("foo.new", _))
      .WillOnce(WithArg<1>(CopyToSecureBlob(&bytes)));
  EXPECT_CALL(platform, Rename("foo.new", "foo"))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, ReadFile("foo", _))
      .WillOnce(WithArg<1>(CopyFromSecureBlob(&bytes)));

  SecureBlob key("key", 3);
  EXPECT_TRUE(keyset.Encrypt(key));
  EXPECT_TRUE(keyset.Save("foo"));

  VaultKeyset new_keyset;
  new_keyset.Initialize(&platform, &crypto);
  EXPECT_TRUE(new_keyset.Load("foo"));
  EXPECT_TRUE(new_keyset.Decrypt(key));
}

TEST_F(VaultKeysetTest, BadSavePerms) {
  MockPlatform platform;
  Crypto crypto(&platform);
  VaultKeyset keyset;
  keyset.Initialize(&platform, &crypto);

  keyset.CreateRandom();
  SecureBlob bytes;

  EXPECT_CALL(platform, WriteFile("foo.new", _))
      .WillOnce(WithArg<1>(CopyToSecureBlob(&bytes)));
  EXPECT_CALL(platform, SetMask(0066))
    .WillOnce(Return(0022));
  EXPECT_CALL(platform, SetMask(0022))
    .WillOnce(Return(0066));
  EXPECT_CALL(platform, DeleteFile("foo.new", false))
      .WillOnce(Return(true));

  SecureBlob key("key", 3);
  EXPECT_TRUE(keyset.Encrypt(key));
  EXPECT_FALSE(keyset.Save("foo"));
}

// TODO(wad) Mock crypto.cc to test En/decrypt failures.

}  // namespace cryptohome
