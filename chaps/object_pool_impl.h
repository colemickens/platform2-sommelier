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
#include <base/memory/scoped_ptr.h>
#include <base/synchronization/lock.h>
#include <base/synchronization/waitable_event.h>

#include "chaps/object_store.h"

namespace chaps {

class ChapsFactory;
class HandleGenerator;
class ObjectImporter;

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
                 ObjectStore* store,
                 ObjectImporter* importer);
  virtual ~ObjectPoolImpl();
  virtual bool Init();
  virtual bool GetInternalBlob(int blob_id, std::string* blob);
  virtual bool SetInternalBlob(int blob_id, const std::string& blob);
  virtual bool SetEncryptionKey(const std::string& key);
  virtual bool Insert(Object* object);
  virtual bool Import(Object* object);
  virtual bool Delete(const Object* object);
  virtual bool DeleteAll();
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
  bool Parse(const ObjectBlob& object_blob, Object* object);
  bool Serialize(const Object* object, ObjectBlob* serialized);
  bool LoadBlobs(const std::map<int, ObjectBlob>& object_blobs);
  bool LoadPublicObjects();
  bool LoadPrivateObjects();
  void WaitForPrivateObjects();

  // Allows us to quickly check whether an object exists in the pool.
  ObjectSet objects_;
  HandleObjectMap handle_object_map_;
  ChapsFactory* factory_;
  HandleGenerator* handle_generator_;
  scoped_ptr<ObjectStore> store_;
  scoped_ptr<ObjectImporter> importer_;
  bool is_private_loaded_;
  base::Lock lock_;
  base::WaitableEvent private_loaded_event_;
  bool finish_import_required_;

  DISALLOW_COPY_AND_ASSIGN(ObjectPoolImpl);
};

}  // namespace

#endif  // CHAPS_OBJECT_POOL_IMPL_H
