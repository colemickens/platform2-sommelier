// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_STORE_FAKE_H_
#define CHAPS_OBJECT_STORE_FAKE_H_

#include "chaps/object_store.h"

#include <map>
#include <string>

namespace chaps {

// A fake object store implementation which just stores blobs in memory.
class ObjectStoreFake : public ObjectStore {
 public:
  ObjectStoreFake() : last_handle_(0) {}
  virtual ~ObjectStoreFake() {}
  virtual bool GetInternalBlob(int blob_id, std::string* blob) {
    if (internal_blobs_.find(blob_id) == internal_blobs_.end())
      return false;
    *blob = internal_blobs_[blob_id];
    return true;
  }
  virtual bool SetInternalBlob(int blob_id, const std::string& blob) {
    internal_blobs_[blob_id] = blob;
    return true;
  }
  virtual bool SetEncryptionKey(const std::string& key) {
    return true;
  }
  virtual bool InsertObjectBlob(const ObjectBlob& blob,
                                int* handle) {
    *handle = ++last_handle_;
    object_blobs_[*handle] = blob;
    return true;
  }
  virtual bool DeleteObjectBlob(int handle) {
    object_blobs_.erase(handle);
    return true;
  }
  virtual bool DeleteAllObjectBlobs() {
    object_blobs_.clear();
    return true;
  }
  virtual bool UpdateObjectBlob(int handle, const ObjectBlob& blob) {
    object_blobs_[handle] = blob;
    return true;
  }
  virtual bool LoadPublicObjectBlobs(std::map<int, ObjectBlob>* blobs) {
    *blobs = object_blobs_;
    return true;
  }
  virtual bool LoadPrivateObjectBlobs(std::map<int, ObjectBlob>* blobs) {
    return true;
  }

 private:
  int last_handle_;
  std::map<int, std::string> internal_blobs_;
  std::map<int, ObjectBlob> object_blobs_;
};

}  // namespace chaps

#endif  // CHAPS_OBJECT_STORE_FAKE_H_
