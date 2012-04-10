// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_pool_impl.h"

#include <map>
#include <string>
#include <tr1/memory>
#include <vector>

#include <base/logging.h>
#include <base/synchronization/lock.h>
#include <base/synchronization/waitable_event.h>

#include "chaps/attributes.pb.h"
#include "chaps/chaps.h"
#include "chaps/chaps_factory.h"
#include "chaps/chaps_utility.h"
#include "chaps/handle_generator.h"
#include "chaps/object.h"
#include "chaps/object_importer.h"
#include "chaps/object_store.h"

using base::AutoLock;
using base::AutoUnlock;
using std::map;
using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace chaps {

ObjectPoolImpl::ObjectPoolImpl(ChapsFactory* factory,
                               HandleGenerator* handle_generator,
                               ObjectStore* store,
                               ObjectImporter* importer)
    : factory_(factory),
      handle_generator_(handle_generator),
      store_(store),
      importer_(importer),
      is_private_loaded_(false),
      private_loaded_event_(true, false),  // Manual reset, not signaled.
      finish_import_required_(false) {}

ObjectPoolImpl::~ObjectPoolImpl() {}

bool ObjectPoolImpl::Init() {
  AutoLock lock(lock_);
  if (store_.get()) {
    if (!LoadPublicObjects())
      return false;
    // Import legacy objects. The existence of the 'imported' blob indicates
    // that legacy objects have already been imported. The contents of this blob
    // are ignored.
    AutoUnlock unlock(lock_);
    string imported_blob;
    if (importer_.get() && !GetInternalBlob(kImportedTracker, &imported_blob)) {
      finish_import_required_ = importer_->ImportObjects(this);
      if (!SetInternalBlob(kImportedTracker, imported_blob)) {
        LOG(WARNING) << "Failed to set the import tracker.";
      }
    }
  } else {
    // There are no objects to load.
    is_private_loaded_ = true;
    private_loaded_event_.Signal();
  }
  return true;
}

bool ObjectPoolImpl::GetInternalBlob(int blob_id, string* blob) {
  AutoLock lock(lock_);
  if (store_.get())
    return store_->GetInternalBlob(blob_id, blob);
  return false;
}

bool ObjectPoolImpl::SetInternalBlob(int blob_id, const string& blob) {
  AutoLock lock(lock_);
  if (store_.get())
    return store_->SetInternalBlob(blob_id, blob);
  return false;
}

bool ObjectPoolImpl::SetEncryptionKey(const string& key) {
  AutoLock lock(lock_);
  if (store_.get()) {
    if (!store_->SetEncryptionKey(key))
      return false;
    // Once we have the encryption key we can load private objects.
    if (!LoadPrivateObjects())
      LOG(WARNING) << "Failed to load private objects.";
    if (finish_import_required_) {
      CHECK(importer_.get());
      // Unlock because FinishImportAsync inserts objects into this pool.
      AutoUnlock unlock(lock_);
      if (!importer_->FinishImportAsync(this))
        LOG(WARNING) << "Failed to finish importing objects.";
    }
  }
  // Signal any callers waiting for private objects that they're ready.
  is_private_loaded_ = true;
  private_loaded_event_.Signal();
  return true;
}

bool ObjectPoolImpl::Insert(Object* object) {
  // If it's a private object we need to wait until private objects have been
  // loaded.
  if (object->IsPrivate() && !is_private_loaded_) {
    AutoLock lock(lock_);
    WaitForPrivateObjects();
  }
  return Import(object);
}

bool ObjectPoolImpl::Import(Object* object) {
  AutoLock lock(lock_);
  if (objects_.find(object) != objects_.end())
    return false;
  if (store_.get()) {
    ObjectBlob serialized;
    if (!Serialize(object, &serialized))
      return false;
    // Parsing the serialized blob will normalize the object attribute values.
    // e.g. If the caller specified 32-bits for a CK_ULONG on a 64-bit system,
    // the value will be resized correctly.
    if (!Parse(serialized, object))
      return false;
    int store_id;
    if (!store_->InsertObjectBlob(serialized, &store_id))
      return false;
    object->set_store_id(store_id);
  }
  object->set_handle(handle_generator_->CreateHandle());
  objects_.insert(object);
  handle_object_map_[object->handle()] = shared_ptr<const Object>(object);
  return true;
}

bool ObjectPoolImpl::Delete(const Object* object) {
  AutoLock lock(lock_);
  if (objects_.find(object) == objects_.end())
    return false;
  if (store_.get()) {
    // If it's a private object we need to wait until private objects have been
    // loaded.
    if (object->IsPrivate() && !is_private_loaded_)
      WaitForPrivateObjects();
    if (!store_->DeleteObjectBlob(object->store_id()))
      return false;
  }
  handle_object_map_.erase(object->handle());
  objects_.erase(object);
  return true;
}

bool ObjectPoolImpl::Find(const Object* search_template,
                          vector<const Object*>* matching_objects) {
  AutoLock lock(lock_);
  // If we're looking for private objects we need to wait until private objects
  // have been loaded.
  if (((search_template->IsAttributePresent(CKA_PRIVATE) &&
      search_template->IsPrivate()) ||
      (search_template->IsAttributePresent(CKA_CLASS) &&
      search_template->GetObjectClass() == CKO_PRIVATE_KEY)) &&
      !is_private_loaded_)
    WaitForPrivateObjects();
  for (ObjectSet::iterator it = objects_.begin(); it != objects_.end(); ++it) {
    if (Matches(search_template, *it))
      matching_objects->push_back(*it);
  }
  return true;
}

bool ObjectPoolImpl::FindByHandle(int handle, const Object** object) {
  AutoLock lock(lock_);
  CHECK(object);
  HandleObjectMap::iterator it = handle_object_map_.find(handle);
  if (it == handle_object_map_.end())
    return false;
  *object = it->second.get();
  return true;
}

Object* ObjectPoolImpl::GetModifiableObject(const Object* object) {
  return const_cast<Object*>(object);
}

bool ObjectPoolImpl::Flush(const Object* object) {
  AutoLock lock(lock_);
  if (objects_.find(object) == objects_.end())
    return false;
  if (store_.get()) {
    ObjectBlob serialized;
    if (!Serialize(object, &serialized))
      return false;
    // If it's a private object we need to wait until private objects have been
    // loaded.
    if (object->IsPrivate() && !is_private_loaded_)
      WaitForPrivateObjects();
    if (!store_->UpdateObjectBlob(object->store_id(), serialized))
      return false;
  }
  return true;
}

bool ObjectPoolImpl::Matches(const Object* object_template,
                             const Object* object) {
  const AttributeMap* attributes = object_template->GetAttributeMap();
  AttributeMap::const_iterator it;
  for (it = attributes->begin(); it != attributes->end(); ++it) {
    if (!object->IsAttributePresent(it->first))
      return false;
    if (it->second != object->GetAttributeString(it->first))
      return false;
  }
  return true;
}

bool ObjectPoolImpl::Parse(const ObjectBlob& object_blob, Object* object) {
  AttributeList attribute_list;
  if (!attribute_list.ParseFromString(object_blob.blob)) {
    LOG(ERROR) << "Failed to parse proto-buffer.";
    return false;
  }
  for (int i = 0; i < attribute_list.attribute_size(); ++i) {
    const Attribute& attribute = attribute_list.attribute(i);
    if (!attribute.has_value()) {
      LOG(WARNING) << "No value found for attribute: " << attribute.type();
      continue;
    }
    object->SetAttributeString(attribute.type(), attribute.value());
    // Correct the length of integral attributes since they may have been
    // serialized with a different sizeof(CK_ULONG).
    if (IsIntegralAttribute(attribute.type() &&
        attribute.value().length() != sizeof(CK_ULONG))) {
      int int_value = object->GetAttributeInt(attribute.type(), 0);
      object->SetAttributeInt(attribute.type(), int_value);
    }
    if (attribute.type() == CKA_PRIVATE &&
        object->IsPrivate() != object_blob.is_private) {
      // Assume this object has been tampered with.
      LOG(ERROR) << "Privacy attribute mismatch.";
      return false;
    }
  }
  return true;
}

bool ObjectPoolImpl::Serialize(const Object* object, ObjectBlob* serialized) {
  const AttributeMap* attribute_map = object->GetAttributeMap();
  AttributeMap::const_iterator it;
  AttributeList attribute_list;
  for (it = attribute_map->begin(); it != attribute_map->end(); ++it) {
    Attribute* next = attribute_list.add_attribute();
    next->set_type(it->first);
    next->set_length(it->second.length());
    next->set_value(it->second);
  }
  if (!attribute_list.SerializeToString(&serialized->blob)) {
    LOG(ERROR) << "Failed to serialize object.";
    return false;
  }
  serialized->is_private = object->IsPrivate();
  return true;
}

bool ObjectPoolImpl::LoadBlobs(const map<int, ObjectBlob>& object_blobs) {
  map<int, ObjectBlob>::const_iterator it;
  for (it = object_blobs.begin(); it != object_blobs.end(); ++it) {
    shared_ptr<Object> object(factory_->CreateObject());
    // An object that is not parsable will be ignored.
    if (Parse(it->second, object.get())) {
      object->set_handle(handle_generator_->CreateHandle());
      object->set_store_id(it->first);
      objects_.insert(object.get());
      handle_object_map_[object->handle()] = object;
    } else {
      LOG(WARNING) << "Object not parsable: " << it->first;
    }
  }
  return true;
}

bool ObjectPoolImpl::LoadPublicObjects() {
  CHECK(store_.get());
  map<int, ObjectBlob> object_blobs;
  if (!store_->LoadPublicObjectBlobs(&object_blobs))
    return false;
  return LoadBlobs(object_blobs);
}

bool ObjectPoolImpl::LoadPrivateObjects() {
  CHECK(store_.get());
  map<int, ObjectBlob> object_blobs;
  if (!store_->LoadPrivateObjectBlobs(&object_blobs))
    return false;
  return LoadBlobs(object_blobs);
}

void ObjectPoolImpl::WaitForPrivateObjects() {
  AutoUnlock unlock(lock_);
  LOG(INFO) << "Waiting for private objects to be loaded.";
  private_loaded_event_.Wait();
  LOG(INFO) << "Done waiting for private objects.";
}

}  // namespace chaps
