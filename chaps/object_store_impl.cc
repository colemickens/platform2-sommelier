// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_store_impl.h"

#include <map>
#include <string>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>
#include <chromeos/utility.h>
#include <leveldb/db.h>
#include <leveldb/env.h>
#include <leveldb/memenv.h>

#include "chaps/chaps_utility.h"
#include "pkcs11/cryptoki.h"

using std::map;
using std::string;

namespace chaps {

const char ObjectStoreImpl::kInternalBlobKeyPrefix[] = "InternalBlob";
const char ObjectStoreImpl::kObjectBlobKeyPrefix[] = "ObjectBlob";
const char ObjectStoreImpl::kBlobKeySeparator[] = "&";
const char ObjectStoreImpl::kDatabaseVersionKey[] = "DBVersion";
const char ObjectStoreImpl::kIDTrackerKey[] = "NextBlobID";
const int ObjectStoreImpl::kAESKeySizeBytes = 32;
const int ObjectStoreImpl::kHMACSizeBytes = 64;
const char ObjectStoreImpl::kDatabaseDirectory[] = "database";

ObjectStoreImpl::ObjectStoreImpl() {}

ObjectStoreImpl::~ObjectStoreImpl() {
  // TODO(dkrahn): Use SecureBlob. See crosbug.com/27681.
  chromeos::SecureMemset(const_cast<char*>(key_.data()), 0, key_.length());
}

bool ObjectStoreImpl::Init(const FilePath& database_path) {
  LOG(INFO) << "Opening database in: " << database_path.value();
  leveldb::Options options;
  options.create_if_missing = true;
  options.paranoid_checks = true;
  if (database_path.value() == ":memory:") {
    // Memory only environment, useful for testing.
    LOG(INFO) << "Using memory-only environment.";
    env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    options.env = env_.get();
  }
  FilePath database_name = database_path.Append(kDatabaseDirectory);
  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options,
                                             database_name.value(),
                                             &db);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to open database: " << status.ToString();
    // We don't want to risk using a database that has been corrupted.
    status = leveldb::DestroyDB(database_name.value(), options);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to destroy database: " << status.ToString();
      return false;
    }
    LOG(WARNING) << "Recreating database from scratch.";
    // Now retry the open.
    status = leveldb::DB::Open(options, database_name.value(), &db);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to open database again: " << status.ToString();
      return false;
    }
  }
  db_.reset(db);
  int version = 0;
  if (!ReadInt(kDatabaseVersionKey, &version)) {
    if (!WriteInt(kIDTrackerKey, 1)) {
      LOG(ERROR) << "Failed to initialize the blob ID tracker.";
      return false;
    }
    if (!WriteInt(kDatabaseVersionKey, 1)) {
      LOG(ERROR) << "Failed to initialize the database version.";
      return false;
    }
  }
  return true;
}

bool ObjectStoreImpl::GetInternalBlob(int blob_id, string* blob) {
  if (!ReadBlob(CreateBlobKey(true, blob_id), blob)) {
    LOG(ERROR) << "Failed to read internal blob: " << blob_id;
    return false;
  }
  return true;
}

bool ObjectStoreImpl::SetInternalBlob(int blob_id, const string& blob) {
  if (!WriteBlob(CreateBlobKey(true, blob_id), blob)) {
    LOG(ERROR) << "Failed to write internal blob: " << blob_id;
    return false;
  }
  return true;
}

bool ObjectStoreImpl::SetEncryptionKey(const string& key) {
  if (key.length() != static_cast<size_t>(kAESKeySizeBytes)) {
    LOG(ERROR) << "Unexpected key size: " << key.length();
    return false;
  }
  key_ = key;
  return true;
}

bool ObjectStoreImpl::InsertObjectBlob(const string& blob, int* handle) {
  if (key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  if (!GetNextID(handle)) {
    LOG(ERROR) << "Failed to generate blob identifier.";
    return false;
  }
  return UpdateObjectBlob(*handle, blob);
}

bool ObjectStoreImpl::DeleteObjectBlob(int handle) {
  leveldb::WriteOptions options;
  options.sync = true;
  leveldb::Status status = db_->Delete(options, CreateBlobKey(false, handle));
  if (!status.ok()) {
    LOG(ERROR) << "Failed to delete blob: " << status.ToString();
    return false;
  }
  return true;
}

bool ObjectStoreImpl::UpdateObjectBlob(int handle, const string& blob) {
  if (key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  string encrypted_blob;
  if (!Encrypt(blob, &encrypted_blob)) {
    LOG(ERROR) << "Failed to encrypt object blob.";
    return false;
  }
  if (!WriteBlob(CreateBlobKey(false, handle), encrypted_blob)) {
    LOG(ERROR) << "Failed to write object blob.";
  }
  return true;
}

bool ObjectStoreImpl::LoadAllObjectBlobs(map<int, string>* blobs) {
  if (key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    bool is_internal = true;
    int id = 0;
    if (ParseBlobKey(it->key().ToString(), &is_internal, &id) && !is_internal) {
      string blob;
      if (!Decrypt(it->value().ToString(), &blob)) {
        LOG(WARNING) << "Failed to decrypt object blob.";
        continue;
      }
      (*blobs)[id] = blob;
    }
  }
  return true;
}

bool ObjectStoreImpl::Encrypt(const string& plain_text,
                              string* cipher_text) {
  if (key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  // Append a MAC to the plain-text before encrypting.
  return RunCipher(true, key_, string(), AppendHMAC(plain_text), cipher_text);
}

bool ObjectStoreImpl::Decrypt(const string& cipher_text,
                              string* plain_text) {
  if (key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  // Recover the IV from the input.
  string plain_text_with_hmac;
  if (!RunCipher(false, key_, string(), cipher_text, &plain_text_with_hmac))
    return false;
  // Check the MAC that was appended before encrypting.
  if (!VerifyAndStripHMAC(plain_text_with_hmac, plain_text))
    return false;
  return true;
}

string ObjectStoreImpl::AppendHMAC(const string& input) {
  return input + HmacSha512(input, key_);
}

bool ObjectStoreImpl::VerifyAndStripHMAC(const string& input,
                                         string* stripped) {
  if (input.size() < static_cast<size_t>(kHMACSizeBytes)) {
    LOG(ERROR) << "Failed to verify blob integrity.";
    return false;
  }
  *stripped = input.substr(0, input.size() - kHMACSizeBytes);
  string hmac = input.substr(input.size() - kHMACSizeBytes);
  if (hmac != HmacSha512(*stripped, key_)) {
    LOG(ERROR) << "Failed to verify blob integrity.";
    return false;
  }
  return true;
}

string ObjectStoreImpl::CreateBlobKey(bool is_internal, int blob_id) {
  const char* prefix =
      is_internal ? kInternalBlobKeyPrefix : kObjectBlobKeyPrefix;
  return base::StringPrintf("%s%s%d", prefix, kBlobKeySeparator, blob_id);
}

bool ObjectStoreImpl::ParseBlobKey(const string& key,
                                   bool* is_internal,
                                   int* blob_id) {
  size_t index = key.rfind(kBlobKeySeparator);
  if (index == string::npos)
    return false;
  if (!base::StringToInt(key.begin() + (index + 1), key.end(), blob_id))
    return false;
  string prefix = key.substr(0, index);
  if (prefix != kInternalBlobKeyPrefix && prefix != kObjectBlobKeyPrefix)
    return false;
  *is_internal = (prefix == kInternalBlobKeyPrefix);
  return true;
}

bool ObjectStoreImpl::GetNextID(int* next_id) {
  if (!ReadInt(kIDTrackerKey, next_id)) {
    LOG(ERROR) << "Failed to read ID tracker.";
    return false;
  }
  if (*next_id == INT_MAX) {
    LOG(ERROR) << "Object ID overflow.";
    return false;
  }
  if (!WriteInt(kIDTrackerKey, *next_id + 1)) {
    LOG(ERROR) << "Failed to write ID tracker.";
    return false;
  }
  return true;
}

bool ObjectStoreImpl::ReadBlob(const string& key, string* value) {
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, value);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to read value from database: " << status.ToString();
    return false;
  }
  return true;
}

bool ObjectStoreImpl::ReadInt(const string& key, int* value) {
  string value_string;
  if (!ReadBlob(key, &value_string))
    return false;
  if (!base::StringToInt(value_string, value)) {
    LOG(ERROR) << "Invalid integer value.";
    return false;
  }
  return true;
}

bool ObjectStoreImpl::WriteBlob(const string& key, const string& value) {
  leveldb::WriteOptions options;
  options.sync = true;
  leveldb::Status status = db_->Put(options, key, value);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to write value to database: " << status.ToString();
    return false;
  }
  return true;
}

bool ObjectStoreImpl::WriteInt(const string& key, int value) {
  return WriteBlob(key, base::IntToString(value));
}

}  // namespace chaps
