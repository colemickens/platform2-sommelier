// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POOL_IMPL_H
#define CHAPS_OBJECT_POOL_IMPL_H

#include "chaps/object_pool.h"

#include <map>
#include <string>
#include <tr1/memory>
#include <set>
#include <vector>

#include <base/basictypes.h>
#include <base/scoped_ptr.h>

namespace chaps {

class ChapsFactory;
class HandleGenerator;
class ObjectStore;

// Key: Object handle.
// Value: Object shared pointer.
typedef std::map<int, std::tr1::shared_ptr<const Object> > HandleObjectMap;
typedef std::set<const Object*> ObjectSet;

class ObjectPoolImpl : public ObjectPool {
 public:
  // The 'factory' and 'handle_generator' pointers are not owned by the object
  // pool. They must remain valid for the entire life of the ObjectPoolImpl
  // instance. If the object pool is not persistent, 'store' should be NULL.
  // Otherwise, 'store' will be owned by (and later deleted by) the object pool.
  ObjectPoolImpl(ChapsFactory* factory,
                 HandleGenerator* handle_generator,
                 ObjectStore* store);
  virtual ~ObjectPoolImpl();
  virtual bool GetInternalBlob(int blob_id, std::string* blob);
  virtual bool SetInternalBlob(int blob_id, const std::string& blob);
  virtual bool SetEncryptionKey(const std::string& key);
  virtual bool Insert(Object* object);
  virtual bool Delete(const Object* object);
  virtual bool Find(const Object* search_template,
                    std::vector<const Object*>* matching_objects);
  virtual bool FindByHandle(int handle, const Object** object);
  virtual Object* GetModifiableObject(const Object* object);
  virtual bool Flush(const Object* object);

 private:
  // An object matches a template when it holds values for all template
  // attributes and those values match the template values. This function
  // returns true if the given object matches the given template.
  bool Matches(const Object* object_template, const Object* object);
  bool Parse(const std::string& object_blob, Object* object);
  bool Serialize(const Object* object, std::string* serialized);
  bool LoadObjects();

  // Allows us to quickly check whether an object exists in the pool.
  ObjectSet objects_;
  HandleObjectMap handle_object_map_;
  ChapsFactory* factory_;
  HandleGenerator* handle_generator_;
  scoped_ptr<ObjectStore> store_;

  DISALLOW_COPY_AND_ASSIGN(ObjectPoolImpl);
};

}  // namespace

#endif  // CHAPS_OBJECT_POOL_IMPL_H
