// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for VaultKeyset.

#include "vault_keyset.h"

#include <base/logging.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>

#include "crypto.h"
#include "cryptohome_common.h"

namespace cryptohome {
using std::string;

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
      if (memcmp(&haystack[start], &needle[0], needle.size()) == 0) {
        return true;
      }
    }
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VaultKeysetTest);
};

TEST_F(VaultKeysetTest, AllocateRandom) {
  // Check that allocating a random VaultKeyset works
  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(Crypto());

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
  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(Crypto());

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
  VaultKeyset vault_keyset;
  vault_keyset.CreateRandom(Crypto());

  SecureBlob blob;
  EXPECT_TRUE(vault_keyset.ToKeysBlob(&blob));

  VaultKeyset new_vault_keyset;
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

} // namespace cryptohome
