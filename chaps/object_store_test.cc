// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_store_impl.h"

#include <map>
#include <string>

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>
#include <openssl/err.h>

using std::map;
using std::string;

namespace chaps {

class TestObjectStoreEncryption : public ::testing::Test {
 public:
  bool TestEncryption(ObjectStoreImpl& store,
                      const string& input) {
    string encrypted;
    if (!store.Encrypt(input, &encrypted))
      return false;
    string decrypted;
    if (!store.Decrypt(encrypted, &decrypted))
      return false;
    return (input == decrypted);
  }
};

TEST_F(TestObjectStoreEncryption, EncryptionInit) {
  ObjectStoreImpl store;
  string input(10, 0x00), output;
  EXPECT_FALSE(store.Encrypt(input, &output));
  EXPECT_FALSE(store.Decrypt(input, &output));
  EXPECT_FALSE(store.SetEncryptionKey(string()));
  EXPECT_FALSE(store.SetEncryptionKey(string(16, 0xAA)));
  EXPECT_FALSE(store.SetEncryptionKey(string(31, 0xAA)));
  EXPECT_FALSE(store.SetEncryptionKey(string(33, 0xAA)));
  EXPECT_TRUE(store.SetEncryptionKey(string(32, 0xAA)));
  EXPECT_TRUE(TestEncryption(store, input));
}

TEST_F(TestObjectStoreEncryption, Encryption) {
  ObjectStoreImpl store;
  string key(32, 0xAA);
  ASSERT_TRUE(store.SetEncryptionKey(key));
  string blob(64, 0xBB);
  // On AES block boundary.
  EXPECT_TRUE(TestEncryption(store, blob));
  // Not on AES block boundary.
  EXPECT_TRUE(TestEncryption(store, string(21, 0xCC)));
  // One over AES block boundary.
  EXPECT_TRUE(TestEncryption(store, string(33, 0xDD)));
  // One under AES block boundary.
  EXPECT_TRUE(TestEncryption(store, string(31, 0xEE)));
  // Zero length input.
  EXPECT_TRUE(TestEncryption(store, string()));
  // Test random IV: two identical blobs should have different cipher texts.
  string encrypted1;
  EXPECT_TRUE(store.Encrypt(blob, &encrypted1));
  string encrypted2;
  EXPECT_TRUE(store.Encrypt(blob, &encrypted2));
  EXPECT_TRUE(encrypted1 != encrypted2);
  string decrypted1;
  EXPECT_TRUE(store.Decrypt(encrypted1, &decrypted1));
  EXPECT_TRUE(decrypted1 == blob);
  string decrypted2;
  EXPECT_TRUE(store.Decrypt(encrypted2, &decrypted2));
  EXPECT_TRUE(decrypted2 == blob);
  // Invalid decrypt.
  EXPECT_FALSE(store.Decrypt(blob, &decrypted1));
  // Test corrupted IV.
  encrypted1[encrypted1.size()-1]++;
  EXPECT_TRUE(store.Decrypt(encrypted1, &decrypted1));
  EXPECT_FALSE(decrypted1 == blob);
  // Test corrupted cipher text.
  encrypted2[0]++;
  EXPECT_TRUE(store.Decrypt(encrypted2, &decrypted2));
  EXPECT_FALSE(decrypted2 == blob);
}

TEST_F(TestObjectStoreEncryption, CBCMode) {
  ObjectStoreImpl store;
  string key(32, 0xAA);
  ASSERT_TRUE(store.SetEncryptionKey(key));
  string two_identical_blocks(32, 0xBB);
  string encrypted;
  EXPECT_TRUE(store.Encrypt(two_identical_blocks, &encrypted));
  string encrypted_block1 = encrypted.substr(0, 16);
  string encrypted_block2 = encrypted.substr(16, 16);
  EXPECT_FALSE(encrypted_block1 == encrypted_block2);
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ERR_load_crypto_strings();
  return RUN_ALL_TESTS();
}
