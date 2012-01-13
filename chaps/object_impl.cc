// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_impl.h"

#include <string>

#include <base/logging.h>

#include "chaps/chaps_factory.h"
#include "chaps/chaps_utility.h"
#include "chaps/object_policy.h"
#include "pkcs11/cryptoki.h"

using std::string;

namespace chaps {

ObjectImpl::ObjectImpl(ChapsFactory* factory) : factory_(factory),
                                                stage_(kCreate) {}
ObjectImpl::~ObjectImpl() {}

ObjectStage ObjectImpl::GetStage() const {
  return stage_;
}

int ObjectImpl::GetSize() const {
  AttributeMap::const_iterator it;
  int size = 0;
  for (it = attributes_.begin(); it != attributes_.end(); ++it) {
    // Estimate 12 bytes of overhead per attribute.  This should allow storage
    // of type and length info and some alignment bytes.  Depending on the
    // persistence model, this may not be accurate.
    size += (12 + it->second.length());
  }
  return size;
}

CK_OBJECT_CLASS ObjectImpl::GetObjectClass() const {
  return GetAttributeInt(CKA_CLASS, -1);
}

bool ObjectImpl::IsTokenObject() const {
  return GetAttributeBool(CKA_TOKEN, false);
}

bool ObjectImpl::IsModifiable() const {
  return GetAttributeBool(CKA_MODIFIABLE, false);
}

bool ObjectImpl::IsPrivate() const {
  return GetAttributeBool(CKA_PRIVATE, true);
}

CK_RV ObjectImpl::FinalizeNewObject() {
  if (!IsAttributePresent(CKA_CLASS)) {
    LOG(ERROR) << "Missing object class attribute.";
    return CKR_TEMPLATE_INCOMPLETE;
  }
  policy_.reset(factory_->CreateObjectPolicy(GetObjectClass()));
  CHECK(policy_.get());
  policy_->Init(this);
  AttributeMap::iterator it;
  for (it = attributes_.begin(); it != attributes_.end(); ++it) {
    CK_RV result = policy_->IsModifyAllowed(it->first, it->second);
    if (result != CKR_OK)
      return result;
  }
  policy_->SetDefaultAttributes();
  if (!policy_->IsObjectComplete())
    return CKR_TEMPLATE_INCOMPLETE;
  stage_ = kModify;
  return CKR_OK;
}

CK_RV ObjectImpl::Copy(const Object* original) {
  stage_ = kCopy;
  attributes_ = *original->GetAttributeMap();
  return CKR_OK;
}

CK_RV ObjectImpl::GetAttributes(CK_ATTRIBUTE_PTR attributes,
                                int num_attributes) const {
  CK_RV result = CKR_OK;
  AttributeMap::const_iterator it;
  for (int i = 0; i < num_attributes; ++i) {
    it = attributes_.find(attributes[i].type);
    if (it == attributes_.end()) {
      LOG(ERROR) << "Attribute does not exist: "
                 << AttributeToString(attributes[i].type);
      result = CKR_ATTRIBUTE_TYPE_INVALID;
      attributes[i].ulValueLen = -1;
    } else if (policy_.get() && !policy_->IsReadAllowed(attributes[i].type)) {
      LOG(ERROR) << "Attribute is sensitive: "
                 << AttributeToString(attributes[i].type);
      result = CKR_ATTRIBUTE_SENSITIVE;
      attributes[i].ulValueLen = -1;
    } else if (attributes[i].pValue == NULL) {
      attributes[i].ulValueLen = it->second.length();
    } else if (attributes[i].ulValueLen < it->second.length()) {
      result = CKR_BUFFER_TOO_SMALL;
      attributes[i].ulValueLen = -1;
    } else {
      attributes[i].ulValueLen = it->second.length();
      memcpy(attributes[i].pValue, it->second.data(), it->second.length());
    }
  }
  return result;
}

CK_RV ObjectImpl::SetAttributes(const CK_ATTRIBUTE_PTR attributes,
                                int num_attributes) {
  for (int i = 0; i < num_attributes; ++i) {
    string value(reinterpret_cast<const char*>(attributes[i].pValue),
                 attributes[i].ulValueLen);
    if (policy_.get()) {
      CK_RV result = policy_->IsModifyAllowed(attributes[i].type, value);
      if (result != CKR_OK)
        return result;
    }
    attributes_[attributes[i].type] = value;
  }
  return CKR_OK;
}

bool ObjectImpl::IsAttributePresent(CK_ATTRIBUTE_TYPE type) const {
  return (attributes_.find(type) != attributes_.end());
}

bool ObjectImpl::GetAttributeBool(CK_ATTRIBUTE_TYPE type,
                                  bool default_value) const {
  AttributeMap::const_iterator it = attributes_.find(type);
  if (it == attributes_.end())
    return default_value;
  if (it->second.empty())
    return default_value;
  return (it->second[0] != 0);
}

void ObjectImpl::SetAttributeBool(CK_ATTRIBUTE_TYPE type, bool value) {
  attributes_[type] = string(1, value ? 1 : 0);
}

int ObjectImpl::GetAttributeInt(CK_ATTRIBUTE_TYPE type,
                                int default_value) const {
  AttributeMap::const_iterator it = attributes_.find(type);
  if (it == attributes_.end())
    return default_value;
  if (it->second.length() >= sizeof(CK_ULONG)) {
    if (it->second.length() != sizeof(CK_ULONG))
      LOG(WARNING) << "GetAttributeInt: truncating: " << it->second.length()
                   << " --> " << sizeof(CK_ULONG);
    CK_ULONG value = *reinterpret_cast<const CK_ULONG*>(it->second.data());
    return value;
  } else if (it->second.length() >= sizeof(int)) {
    if (it->second.length() != sizeof(int))
      LOG(WARNING) << "GetAttributeInt: truncating: " << it->second.length()
                   << " --> " << sizeof(int);
    int value = *reinterpret_cast<const int*>(it->second.data());
    return value;
  }
  return default_value;
}

void ObjectImpl::SetAttributeInt(CK_ATTRIBUTE_TYPE type, int value) {
  CK_ULONG long_value = value;
  attributes_[type] = string(reinterpret_cast<const char*>(&long_value),
                             sizeof(CK_ULONG));
}

string ObjectImpl::GetAttributeString(CK_ATTRIBUTE_TYPE type) const {
  AttributeMap::const_iterator it = attributes_.find(type);
  if (it != attributes_.end())
    return it->second;
  return string();
}

void ObjectImpl::SetAttributeString(CK_ATTRIBUTE_TYPE type,
                                    const string& value) {
  attributes_[type] = value;
}

void ObjectImpl::RemoveAttribute(CK_ATTRIBUTE_TYPE type) {
  attributes_.erase(type);
}

const AttributeMap* ObjectImpl::GetAttributeMap() const {
  return &attributes_;
}

}  // namespace chaps
