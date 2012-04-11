// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_pool_impl.h"

#include <map>
#include <string>
#include <tr1/memory>
#include <vector>

#include <base/logging.h>

#include "chaps/attributes.pb.h"
#include "chaps/chaps.h"
#include "chaps/chaps_factory.h"
#include "chaps/chaps_utility.h"
#include "chaps/handle_generator.h"
#include "chaps/object.h"
#include "chaps/object_store.h"

using std::map;
using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace chaps {

ObjectPoolImpl::ObjectPoolImpl(ChapsFactory* factory,
                               HandleGenerator* handle_generator,
                               ObjectStore* store)
    : factory_(factory),
      handle_generator_(handle_generator),
      store_(store) {}

ObjectPoolImpl::~ObjectPoolImpl() {}

bool ObjectPoolImpl::GetInternalBlob(int blob_id, string* blob) {
  if (store_.get())
    return store_->GetInternalBlob(blob_id, blob);
  return false;
}

bool ObjectPoolImpl::SetInternalBlob(int blob_id, const string& blob) {
  if (store_.get())
    return store_->SetInternalBlob(blob_id, blob);
  return false;
}

bool ObjectPoolImpl::SetEncryptionKey(const string& key) {
  if (store_.get()) {
    if (!store_->SetEncryptionKey(key))
      return false;
    return LoadObjects();
  }
  return true;
}

bool ObjectPoolImpl::Insert(Object* object) {
  if (objects_.find(object) != objects_.end())
    return false;
  if (store_.get()) {
    string serialized;
    if (!Serialize(object, &serialized))
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
  if (objects_.find(object) == objects_.end())
    return false;
  if (store_.get()) {
    if (!store_->DeleteObjectBlob(object->store_id()))
      return false;
  }
  handle_object_map_.erase(object->handle());
  objects_.erase(object);
  return true;
}

bool ObjectPoolImpl::Find(const Object* search_template,
                          vector<const Object*>* matching_objects) {
  for (ObjectSet::iterator it = objects_.begin(); it != objects_.end(); ++it) {
    if (Matches(search_template, *it))
      matching_objects->push_back(*it);
  }
  return true;
}

bool ObjectPoolImpl::FindByHandle(int handle, const Object** object) {
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
  if (objects_.find(object) == objects_.end())
    return false;
  if (store_.get()) {
    string serialized;
    if (!Serialize(object, &serialized))
      return false;
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

bool ObjectPoolImpl::Parse(const string& object_blob, Object* object) {
  AttributeList attribute_list;
  if (!attribute_list.ParseFromString(object_blob)) {
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
  }
  return true;
}

bool ObjectPoolImpl::Serialize(const Object* object, string* serialized) {
  const AttributeMap* attribute_map = object->GetAttributeMap();
  AttributeMap::const_iterator it;
  AttributeList attribute_list;
  for (it = attribute_map->begin(); it != attribute_map->end(); ++it) {
    Attribute* next = attribute_list.add_attribute();
    next->set_type(it->first);
    next->set_length(it->second.length());
    next->set_value(it->second);
  }
  if (!attribute_list.SerializeToString(serialized)) {
    LOG(ERROR) << "Failed to serialize object.";
    return false;
  }
  return true;
}

bool ObjectPoolImpl::LoadObjects() {
  CHECK(store_.get());
  map<int, string> object_blobs;
  if (!store_->LoadAllObjectBlobs(&object_blobs))
    return false;
  map<int, string>::iterator it;
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

}  // namespace chaps
