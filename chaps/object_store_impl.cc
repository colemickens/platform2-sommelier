// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_store_impl.h"

#include <map>
#include <string>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_piece.h>
#include <base/string_util.h>
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
const char ObjectStoreImpl::kPublicBlobKeyPrefix[] = "PublicBlob";
const char ObjectStoreImpl::kPrivateBlobKeyPrefix[] = "PrivateBlob";
const char ObjectStoreImpl::kBlobKeySeparator[] = "&";
const char ObjectStoreImpl::kDatabaseVersionKey[] = "DBVersion";
const char ObjectStoreImpl::kIDTrackerKey[] = "NextBlobID";
const int ObjectStoreImpl::kAESKeySizeBytes = 32;
const int ObjectStoreImpl::kHMACSizeBytes = 64;
const char ObjectStoreImpl::kDatabaseDirectory[] = "database";
const char ObjectStoreImpl::kCorruptDatabaseDirectory[] = "database_corrupt";
const char ObjectStoreImpl::kObfuscationKey[] = {
    '\x6f', '\xaa', '\x0a', '\xb6', '\x10', '\xc0', '\xa6', '\xe4', '\x07',
    '\x8b', '\x05', '\x1c', '\xd2', '\x8b', '\xac', '\x2d', '\xba', '\x5e',
    '\x14', '\x9c', '\xae', '\x57', '\xfb', '\x04', '\x13', '\x92', '\xc0',
    '\x84', '\x2a', '\xea', '\xf6', '\xfb', '\0'};

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
    // We don't want to risk using a database that has been corrupted. Recreate
    // the database from scratch but save the corrupted database for diagnostic
    // purposes.
    LOG(WARNING) << "Recreating database from scratch. Moving current database "
                 << "to " << kCorruptDatabaseDirectory;
    FilePath corrupt_db_path = database_path.Append(kCorruptDatabaseDirectory);
    file_util::Delete(corrupt_db_path, true);
    if (!file_util::Move(database_name, corrupt_db_path)) {
      LOG(ERROR) << "Failed to move database." << status.ToString();
      return false;
    }
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
  if (!ReadBlob(CreateBlobKey(kInternal, blob_id), blob)) {
    // Don't log this since it happens legitimately when a blob has not yet been
    // set.
    return false;
  }
  return true;
}

bool ObjectStoreImpl::SetInternalBlob(int blob_id, const string& blob) {
  if (!WriteBlob(CreateBlobKey(kInternal, blob_id), blob)) {
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

bool ObjectStoreImpl::InsertObjectBlob(const ObjectBlob& blob,
                                       int* handle) {
  if (blob.is_private && key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  if (!GetNextID(handle)) {
    LOG(ERROR) << "Failed to generate blob identifier.";
    return false;
  }
  blob_type_map_[*handle] = blob.is_private ? kPrivate : kPublic;
  return UpdateObjectBlob(*handle, blob);
}

bool ObjectStoreImpl::DeleteObjectBlob(int handle) {
  leveldb::WriteOptions options;
  options.sync = true;
  string db_key = CreateBlobKey(GetBlobType(handle), handle);
  leveldb::Status status = db_->Delete(options, db_key);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to delete blob: " << status.ToString();
    return false;
  }
  return true;
}

bool ObjectStoreImpl::UpdateObjectBlob(int handle, const ObjectBlob& blob) {
  ObjectBlob encrypted_blob;
  BlobType type = GetBlobType(handle);
  if (blob.is_private != (type == kPrivate)) {
    LOG(ERROR) << "Object privacy mismatch.";
    return false;
  }
  if (!Encrypt(blob, &encrypted_blob)) {
    LOG(ERROR) << "Failed to encrypt object blob.";
    return false;
  }
  if (!WriteBlob(CreateBlobKey(type, handle), encrypted_blob.blob)) {
    LOG(ERROR) << "Failed to write object blob.";
  }
  return true;
}

bool ObjectStoreImpl::LoadPublicObjectBlobs(map<int, ObjectBlob>* blobs) {
  return LoadObjectBlobs(kPublic, blobs);
}

bool ObjectStoreImpl::LoadPrivateObjectBlobs(map<int, ObjectBlob>* blobs) {
  if (key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  return LoadObjectBlobs(kPrivate, blobs);
}

bool ObjectStoreImpl::LoadObjectBlobs(BlobType type,
                                      map<int, ObjectBlob>* blobs) {
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    BlobType it_type;
    int id = 0;
    if (ParseBlobKey(it->key().ToString(), &it_type, &id) && type == it_type) {
      ObjectBlob encrypted_blob;
      encrypted_blob.is_private = (type == kPrivate);
      encrypted_blob.blob = it->value().ToString();
      ObjectBlob blob;
      if (!Decrypt(encrypted_blob, &blob)) {
        LOG(WARNING) << "Failed to decrypt object blob.";
        continue;
      }
      (*blobs)[id] = blob;
      blob_type_map_[id] = type;
    }
  }
  return true;
}

bool ObjectStoreImpl::Encrypt(const ObjectBlob& plain_text,
                              ObjectBlob* cipher_text) {
  if (plain_text.is_private && key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  cipher_text->is_private = plain_text.is_private;
  string key = plain_text.is_private ? key_ : kObfuscationKey;
  // Append a MAC to the plain-text before encrypting.
  return RunCipher(true,
                   key,
                   string(),
                   AppendHMAC(plain_text.blob, key),
                   &cipher_text->blob);
}

bool ObjectStoreImpl::Decrypt(const ObjectBlob& cipher_text,
                              ObjectBlob* plain_text) {
  if (cipher_text.is_private && key_.empty()) {
    LOG(ERROR) << "The store encryption key has not been initialized.";
    return false;
  }
  plain_text->is_private = cipher_text.is_private;
  // Recover the IV from the input.
  string key = cipher_text.is_private ? key_ : kObfuscationKey;
  string plain_text_with_hmac;
  if (!RunCipher(false, key, string(), cipher_text.blob, &plain_text_with_hmac))
    return false;
  // Check the MAC that was appended before encrypting.
  if (!VerifyAndStripHMAC(plain_text_with_hmac, key, &plain_text->blob)) {
    // Due to a past bug, public object MACs may have been generated with the
    // master key.
    if (cipher_text.is_private ||
        !VerifyAndStripHMAC(plain_text_with_hmac, key_, &plain_text->blob)) {
      return false;
    }
  }
  return true;
}

string ObjectStoreImpl::AppendHMAC(const string& input, const string& key) {
  return input + HmacSha512(input, key);
}

bool ObjectStoreImpl::VerifyAndStripHMAC(const string& input,
                                         const string& key,
                                         string* stripped) {
  if (input.size() < static_cast<size_t>(kHMACSizeBytes)) {
    LOG(ERROR) << "Failed to verify blob integrity.";
    return false;
  }
  *stripped = input.substr(0, input.size() - kHMACSizeBytes);
  string hmac = input.substr(input.size() - kHMACSizeBytes);
  if (hmac != HmacSha512(*stripped, key)) {
    LOG(ERROR) << "Failed to verify blob integrity.";
    return false;
  }
  return true;
}

string ObjectStoreImpl::CreateBlobKey(BlobType type, int blob_id) {
  const char* prefix = NULL;
  switch (type) {
    case kInternal:
      prefix = kInternalBlobKeyPrefix;
      break;
    case kPublic:
      prefix = kPublicBlobKeyPrefix;
      break;
    case kPrivate:
      prefix = kPrivateBlobKeyPrefix;
      break;
    default:
      LOG(FATAL) << "Invalid enum value.";
  }
  return base::StringPrintf("%s%s%d", prefix, kBlobKeySeparator, blob_id);
}

bool ObjectStoreImpl::ParseBlobKey(const string& key,
                                   BlobType* type,
                                   int* blob_id) {
  size_t index = key.rfind(kBlobKeySeparator);
  if (index == string::npos) {
    // This isn't a blob.
    return false;
  }
  base::StringPiece key_piece(key.begin() + (index + 1), key.end());
  if (!base::StringToInt(key_piece, blob_id)) {
    LOG(ERROR) << "Invalid blob key id: " << key;
    return false;
  }
  string prefix = key.substr(0, index);
  if (prefix == kInternalBlobKeyPrefix) {
    *type = kInternal;
  } else if (prefix == kPublicBlobKeyPrefix) {
    *type = kPublic;
  } else if (prefix == kPrivateBlobKeyPrefix) {
    *type = kPrivate;
  } else {
    LOG(ERROR) << "Invalid blob key prefix: " << key;
    return false;
  }
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
    if (!status.IsNotFound())
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

ObjectStoreImpl::BlobType ObjectStoreImpl::GetBlobType(int blob_id) {
  map<int, BlobType>::iterator it = blob_type_map_.find(blob_id);
  if (it == blob_type_map_.end())
    return kInternal;
  return it->second;
}

}  // namespace chaps
