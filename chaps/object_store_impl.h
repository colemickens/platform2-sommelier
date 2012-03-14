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
#include <base/scoped_ptr.h>
#include <gtest/gtest.h>
#include <leveldb/db.h>
#include <leveldb/env.h>
#include <openssl/evp.h>

namespace chaps {

// An ObjectStore implementation based on SQLite.
class ObjectStoreImpl : public ObjectStore {
 public:
  ObjectStoreImpl();
  virtual ~ObjectStoreImpl();

  // Initializes the object store with the given database file. The magic file
  // name ":memory:" will cause the store to create a memory-only database which
  // is suitable for testing.
  bool Init(const FilePath& database_file);

  // ObjectStore methods.
  virtual bool GetInternalBlob(int blob_id, std::string* blob);
  virtual bool SetInternalBlob(int blob_id, const std::string& blob);
  virtual bool SetEncryptionKey(const std::string& key);
  virtual bool InsertObjectBlob(const std::string& blob, int* handle);
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

  // Creates and returns a unique database key for a blob.
  std::string CreateBlobKey(bool is_internal, int blob_id);

  // Given a valid blob key (as created by CreateBlobKey), determines whether
  // the blob is internal and the blob id. Returns true on success.
  bool ParseBlobKey(const std::string& key, bool* is_internal, int* blob_id);

  // Computes and returns the next (unused) blob id;
  bool GetNextID(int* next_id);

  // Reads a blob from the database. Returns true on success.
  bool ReadBlob(const std::string& key, std::string* value);

  // Reads an integer from the database. Returns true on success.
  bool ReadInt(const std::string& key, int* value);

  // Writes a blob to the database. Returns true on success.
  bool WriteBlob(const std::string& key, const std::string& value);

  // Writes an integer to the database. Returns true on success.
  bool WriteInt(const std::string& key, int value);

  // These strings are used to construct database keys for blobs. In general the
  // format of a blob database key is: <prefix><separator><id>.
  static const char kInternalBlobKeyPrefix[];
  static const char kObjectBlobKeyPrefix[];
  static const char kBlobKeySeparator[];
  // The key for the database version. The existence of this value indicates the
  // database is not new.
  static const char kDatabaseVersionKey[];
  // The database key for the ID tracker, which always holds a value larger than
  // any object blob ID in use.
  static const char kIDTrackerKey[];
  static const int kAESBlockSizeBytes;
  static const int kAESKeySizeBytes;
  static const int kHMACSizeBytes;

  std::string key_;
  EVP_CIPHER_CTX cipher_context_;
  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::DB> db_;

  friend class TestObjectStoreEncryption;
  FRIEND_TEST(TestObjectStoreEncryption, EncryptionInit);
  FRIEND_TEST(TestObjectStoreEncryption, Encryption);
  FRIEND_TEST(TestObjectStoreEncryption, CBCMode);

  DISALLOW_COPY_AND_ASSIGN(ObjectStoreImpl);
};

}  // namespace chaps

#endif  // CHAPS_OBJECT_STORE_IMPL_H_
