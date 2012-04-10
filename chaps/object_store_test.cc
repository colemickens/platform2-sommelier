// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_store_impl.h"

#include <map>
#include <string>

#include <base/memory/scoped_ptr.h>
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
  string encrypted_ok = encrypted1;
  encrypted1[encrypted1.size()-1]++;
  EXPECT_FALSE(store.Decrypt(encrypted1, &decrypted1));
  // Test corrupted cipher text.
  encrypted1 = encrypted_ok;
  encrypted1[0]++;
  EXPECT_FALSE(store.Decrypt(encrypted1, &decrypted1));
  // Test corrupted hmac.
  encrypted1 = encrypted_ok;
  encrypted1[encrypted1.size()-17]++;
  EXPECT_FALSE(store.Decrypt(encrypted1, &decrypted1));
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

TEST(TestObjectStore, InsertLoad) {
  ObjectStoreImpl store;
  const FilePath::CharType database[] = FILE_PATH_LITERAL(":memory:");
  store.Init(FilePath(database));
  string key(32, 'A');
  EXPECT_TRUE(store.SetEncryptionKey(key));
  map<int,string> objects;
  EXPECT_TRUE(store.LoadAllObjectBlobs(&objects));
  EXPECT_EQ(0, objects.size());
  int handle1;
  string blob1 = "blob1";
  EXPECT_TRUE(store.InsertObjectBlob(blob1, &handle1));
  int handle2;
  string blob2 = "blob2";
  EXPECT_TRUE(store.InsertObjectBlob(blob2, &handle2));
  EXPECT_TRUE(store.LoadAllObjectBlobs(&objects));
  EXPECT_EQ(2, objects.size());
  EXPECT_TRUE(objects.end() != objects.find(handle1));
  EXPECT_TRUE(objects.end() != objects.find(handle2));
  EXPECT_TRUE(blob1 == objects[handle1]);
  EXPECT_TRUE(blob2 == objects[handle2]);
}

TEST(TestObjectStore, UpdateDelete) {
  ObjectStoreImpl store;
  const FilePath::CharType database[] = FILE_PATH_LITERAL(":memory:");
  store.Init(FilePath(database));
  string key(32, 'A');
  EXPECT_TRUE(store.SetEncryptionKey(key));
  int handle1;
  string blob1 = "blob1";
  EXPECT_TRUE(store.InsertObjectBlob(blob1, &handle1));
  map<int,string> objects;
  EXPECT_TRUE(store.LoadAllObjectBlobs(&objects));
  EXPECT_TRUE(blob1 == objects[handle1]);
  string blob2 = "blob2";
  EXPECT_TRUE(store.UpdateObjectBlob(handle1, blob2));
  EXPECT_TRUE(store.LoadAllObjectBlobs(&objects));
  EXPECT_EQ(1, objects.size());
  EXPECT_TRUE(blob2 == objects[handle1]);
  EXPECT_TRUE(store.DeleteObjectBlob(handle1));
  objects.clear();
  EXPECT_TRUE(store.LoadAllObjectBlobs(&objects));
  EXPECT_EQ(0, objects.size());
}

TEST(TestObjectStore, InternalBlobs) {
  ObjectStoreImpl store;
  const FilePath::CharType database[] = FILE_PATH_LITERAL(":memory:");
  store.Init(FilePath(database));
  string blob;
  EXPECT_FALSE(store.GetInternalBlob(1, &blob));
  EXPECT_TRUE(store.SetInternalBlob(1, "blob"));
  EXPECT_TRUE(store.GetInternalBlob(1, &blob));
  EXPECT_TRUE(blob == "blob");
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ERR_load_crypto_strings();
  return RUN_ALL_TESTS();
}
