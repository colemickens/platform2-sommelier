// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_STORE_H
#define CHAPS_OBJECT_STORE_H

#include <map>
#include <string>

#include "pkcs11/cryptoki.h"

namespace chaps {

// An object store provides persistent storage of object blobs and internal
// blobs. All stored blobs are encrypted. Object properties (e.g. object class)
// are not necessarily encrypted.
class ObjectStore {
 public:
  virtual ~ObjectStore() {}
  // These methods get and set internal persistent blobs. If a value has not yet
  // been set for a particular blob, GetInternalBlob will provide an empty
  // string. It is assumed that internal blobs are already encrypted and can be
  // used to bootstrap the encryption of object blobs. Thus, they are not
  // encrypted by the ObjectStore and they can be accessed and modified before
  // SetEncryptionKey is called.
  //   blob_id - The value of this identifier must be managed by the caller.
  //             Only one blob can be set per blob_id (i.e. a subsequent call
  //             to SetInternalBlob with the same blob_id will overwrite the
  //             blob).
  //  blob - The blob data. This will not be encrypted.
  virtual bool GetInternalBlob(int blob_id, std::string* blob) = 0;
  virtual bool SetInternalBlob(int blob_id, const std::string& blob) = 0;
  // SetEncryptionKey sets the encryption key used to encrypt all object blobs.
  // This method must be called before any object blob methods (e.g.
  // InsertObjectBlob, DeleteObjectBlob, ...).
  virtual bool SetEncryptionKey(const std::string& key) = 0;
  // Inserts a new blob.
  virtual bool InsertObjectBlob(const std::string& blob, int* blob_id) = 0;
  // Deletes an existing blob.
  virtual bool DeleteObjectBlob(int blob_id) = 0;
  // Updates (replaces) an existing object blob.
  virtual bool UpdateObjectBlob(int blob_id, const std::string& blob) = 0;
  // Loads all non-internal objects.
  virtual bool LoadAllObjectBlobs(std::map<int, std::string>* blobs) = 0;
};

}  // namespace

#endif  // CHAPS_OBJECT_STORE_H
