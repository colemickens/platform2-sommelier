// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POOL_H
#define CHAPS_OBJECT_POOL_H

#include <string>
#include <vector>

namespace chaps {

class Object;

// An ObjectPool instance manages a collection of objects.  A persistent object
// pool is backed by a database where all object data and object-related
// metadata is stored.
class ObjectPool {
 public:
  virtual ~ObjectPool() {}
  // These methods get and set internal persistent blobs. These internal blobs
  // are for use by Chaps. PKCS #11 applications will not see these when
  // searching for objects. Only persistent implementations need to support
  // internal blobs. Internal blobs do not need to be encrypted.
  //   blob_id - The value of this identifier must be managed by the caller.
  //             Only one blob can be set per blob_id (i.e. a subsequent call
  //             to SetInternalBlob with the same blob_id will overwrite the
  //             blob).
  virtual bool GetInternalBlob(int blob_id, std::string* blob) = 0;
  virtual bool SetInternalBlob(int blob_id, const std::string& blob) = 0;
  // SetKey sets the encryption key for objects in this pool. This is only
  // relevant if the pool is persistent; an object pool has no obligation to
  // encrypt object data in memory.
  virtual bool SetEncryptionKey(const std::string& key) = 0;
  // This method takes ownership of the 'object' pointer on success.
  virtual bool Insert(Object* object) = 0;
  virtual bool Delete(const Object* object) = 0;
  // Finds all objects matching the search template and appends them to the
  // supplied vector.
  virtual bool Find(const Object* search_template,
                    std::vector<const Object*>* matching_objects) = 0;
  // Finds an object by handle. Returns false if the handle does not exist.
  virtual bool FindByHandle(int handle, const Object** object) = 0;
  // Returns a modifiable version of the given object.
  virtual Object* GetModifiableObject(const Object* object) = 0;
  // Flushes a modified object to persistent storage.
  virtual bool Flush(const Object* object) = 0;
};

}  // namespace

#endif  // CHAPS_OBJECT_POOL_H
