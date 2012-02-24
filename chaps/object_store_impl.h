// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_STORE_IMPL_H_
#define CHAPS_OBJECT_STORE_IMPL_H_

#include "chaps/object_store.h"

#include <map>
#include <string>

#include <base/basictypes.h>
#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace chaps {

// An ObjectStore implementation based on SQLite.
class ObjectStoreImpl : public ObjectStore {
 public:
  ObjectStoreImpl();
  virtual ~ObjectStoreImpl();

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
  bool RunCipher(bool is_encrypt,
                 const std::string& input,
                 std::string* output);
  bool Encrypt(const std::string& plain_text, std::string* cipher_text);
  bool Decrypt(const std::string& cipher_text, std::string* plain_text);

  static const int kAESBlockSizeBytes;
  static const int kAESKeySizeBytes;

  std::string key_;
  EVP_CIPHER_CTX cipher_context_;
  const EVP_CIPHER* cipher_;

  friend class TestObjectStoreEncryption;
  FRIEND_TEST(TestObjectStoreEncryption, EncryptionInit);
  FRIEND_TEST(TestObjectStoreEncryption, Encryption);
  FRIEND_TEST(TestObjectStoreEncryption, CBCMode);

  DISALLOW_COPY_AND_ASSIGN(ObjectStoreImpl);
};

}  // namespace chaps

#endif  // CHAPS_OBJECT_STORE_IMPL_H_
