// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_STORE_IMPL_H_
#define CHAPS_OBJECT_STORE_IMPL_H_

#include "chaps/object_store.h"

#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace chaps {

// An ObjectStore implementation based on SQLite.
class ObjectStoreImpl : public ObjectStore {
 public:
  ObjectStoreImpl();
  virtual ~ObjectStoreImpl();

  // Initializes the object store with the given database file.
  bool Init(const FilePath& database_file);

  // ObjectStore methods.
  virtual bool GetInternalBlob(int blob_id, std::string* blob);
  virtual bool SetInternalBlob(int blob_id, const std::string& blob);
  virtual bool SetEncryptionKey(const std::string& key);
  virtual bool InsertObjectBlob(bool is_private,
                                CK_OBJECT_CLASS object_class,
                                const std::string& key_id,
                                const std::string& blob,
                                int* handle);
  virtual bool DeleteObjectBlob(int handle);
  virtual bool UpdateObjectBlob(int handle, const std::string& blob);
  virtual bool LoadAllObjectBlobs(std::map<int, std::string>* blobs);

 private:
  // Encrypts or decrypts object blobs using AES-256-CBC and a random IV which
  // is appended to the cipher text.
  bool RunCipher(bool is_encrypt,
                 const std::string& input,
                 std::string* output);

  // Encrypts an object blob and appends an HMAC.
  bool Encrypt(const std::string& plain_text, std::string* cipher_text);

  // Decrypts an object blob and verifies the HMAC.
  bool Decrypt(const std::string& cipher_text, std::string* plain_text);

  // Computes an HMAC and appends it to the given input.
  std::string AppendHMAC(const std::string& input);

  // Verifies an appended HMAC and strips it from the given input.
  bool VerifyAndStripHMAC(const std::string& input, std::string* stripped);

  static const int kAESBlockSizeBytes;
  static const int kAESKeySizeBytes;
  static const int kHMACSizeBytes;

  std::string key_;
  EVP_CIPHER_CTX cipher_context_;

  friend class TestObjectStoreEncryption;
  FRIEND_TEST(TestObjectStoreEncryption, EncryptionInit);
  FRIEND_TEST(TestObjectStoreEncryption, Encryption);
  FRIEND_TEST(TestObjectStoreEncryption, CBCMode);

  DISALLOW_COPY_AND_ASSIGN(ObjectStoreImpl);
};

}  // namespace chaps

#endif  // CHAPS_OBJECT_STORE_IMPL_H_
