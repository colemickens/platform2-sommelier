// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for VaultKeyset.

#include "vault_keyset.h"

#include <base/logging.h>
#include <chromeos/utility.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "crypto.h"
#include "cryptohome_common.h"
#include "mock_platform.h"

namespace cryptohome {
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
      if (chromeos::SafeMemcmp(&haystack[start], &needle[0],
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
  Crypto crypto;
  VaultKeyset vault_keyset(&platform_, &crypto);
  vault_keyset.CreateRandom();

  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SIZE, vault_keyset.FEK().size());
  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE,
            vault_keyset.FEK_SIG().size());
  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE, vault_keyset.FEK_SALT().size());

  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SIZE, vault_keyset.FNEK().size());
  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE,
            vault_keyset.FNEK_SIG().size());
  EXPECT_EQ(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE, vault_keyset.FNEK_SALT().size());
}

TEST_F(VaultKeysetTest, SerializeTest) {
  // Check that serialize works
  Crypto crypto;
  VaultKeyset vault_keyset(&platform_, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob blob;
  EXPECT_TRUE(vault_keyset.ToKeysBlob(&blob));

  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.FEK()));
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.FEK_SIG()));
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.FEK_SALT()));

  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.FNEK()));
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.FNEK_SIG()));
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(blob, vault_keyset.FNEK_SALT()));
}

TEST_F(VaultKeysetTest, DeserializeTest) {
  // Check that deserialize works
  Crypto crypto;
  VaultKeyset vault_keyset(&platform_, &crypto);
  vault_keyset.CreateRandom();

  SecureBlob blob;
  EXPECT_TRUE(vault_keyset.ToKeysBlob(&blob));

  VaultKeyset new_vault_keyset(&platform_, &crypto);
  new_vault_keyset.FromKeysBlob(blob);

  EXPECT_EQ(vault_keyset.FEK().size(), new_vault_keyset.FEK().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.FEK(),
                                              new_vault_keyset.FEK()));
  EXPECT_EQ(vault_keyset.FEK_SIG().size(), new_vault_keyset.FEK_SIG().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.FEK_SIG(),
                                              new_vault_keyset.FEK_SIG()));
  EXPECT_EQ(vault_keyset.FEK_SALT().size(), new_vault_keyset.FEK_SALT().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.FEK_SALT(),
                                              new_vault_keyset.FEK_SALT()));

  EXPECT_EQ(vault_keyset.FNEK().size(), new_vault_keyset.FNEK().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.FNEK(),
                                              new_vault_keyset.FNEK()));
  EXPECT_EQ(vault_keyset.FNEK_SIG().size(), new_vault_keyset.FNEK_SIG().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.FNEK_SIG(),
                                              new_vault_keyset.FNEK_SIG()));
  EXPECT_EQ(vault_keyset.FNEK_SALT().size(),
            new_vault_keyset.FNEK_SALT().size());
  EXPECT_TRUE(VaultKeysetTest::FindBlobInBlob(vault_keyset.FNEK_SALT(),
                                              new_vault_keyset.FNEK_SALT()));
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
  Crypto crypto;
  VaultKeyset keyset(&platform, &crypto);
  keyset.CreateRandom();
  SecureBlob bytes;

  EXPECT_CALL(platform, WriteFile("foo.new", _))
      .WillOnce(WithArg<1>(CopyToSecureBlob(&bytes)));
  EXPECT_CALL(platform, Rename("foo.new", "foo"))
      .WillOnce(Return(true));
  EXPECT_CALL(platform, ReadFile("foo", _))
      .WillOnce(WithArg<1>(CopyFromSecureBlob(&bytes)));

  SecureBlob key("key", 3);
  EXPECT_TRUE(keyset.Save("foo", key));

  VaultKeyset new_keyset(&platform, &crypto);
  EXPECT_TRUE(new_keyset.Load("foo", key));
}

}  // namespace cryptohome
