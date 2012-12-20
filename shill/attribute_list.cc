// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/attribute_list.h"

#include <ctype.h>
#include <linux/nl80211.h>
#include <netlink/attr.h>
#include <netlink/netlink.h>

#include <iomanip>
#include <map>
#include <string>

#include <base/stl_util.h>

#include "shill/logging.h"
#include "shill/nl80211_attribute.h"
#include "shill/scope_logger.h"

using base::WeakPtr;
using std::map;
using std::string;

namespace shill {

AttributeList::~AttributeList() {
  map<int, Nl80211Attribute *>::iterator i;
  for (i = attributes_.begin(); i != attributes_.end(); ++i) {
    delete i->second;
  }
}

bool AttributeList::CreateAttribute(nl80211_attrs id) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = Nl80211Attribute::NewFromName(id);
  return true;
}

bool AttributeList::CreateAndInitFromNlAttr(nl80211_attrs id,
                                            const nlattr *data) {
  if (!CreateAttribute(id)) {
    return false;
  }
  return attributes_[id]->InitFromNlAttr(data);
}

// U8 Attribute.

bool AttributeList::GetU8AttributeValue(int id, uint8_t *value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeU8)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeU8 exists.";
    return false;
  }
  const Nl80211U8Attribute *attr =
      reinterpret_cast<const Nl80211U8Attribute *>(GetAttribute(id));
  return attr->GetU8Value(value);
}

bool AttributeList::CreateU8Attribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = new Nl80211U8Attribute(id, id_string);
  return true;
}

bool AttributeList::SetU8AttributeValue(int id, uint8_t value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeU8)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeU8 exists.";
    return false;
  }
  Nl80211U8Attribute *attr =
      reinterpret_cast<Nl80211U8Attribute *>(GetAttribute(id));
  return attr->SetU8Value(value);
}


// U16 Attribute.

bool AttributeList::GetU16AttributeValue(int id, uint16_t *value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeU16)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeU16 exists.";
    return false;
  }
  const Nl80211U16Attribute *attr =
      reinterpret_cast<const Nl80211U16Attribute *>(GetAttribute(id));
  return attr->GetU16Value(value);
}

bool AttributeList::CreateU16Attribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = new Nl80211U16Attribute(id, id_string);
  return true;
}

bool AttributeList::SetU16AttributeValue(int id, uint16_t value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeU16)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeU16 exists.";
    return false;
  }
  Nl80211U16Attribute *attr =
      reinterpret_cast<Nl80211U16Attribute *>(GetAttribute(id));
  return attr->SetU16Value(value);
}

// U32 Attribute.

bool AttributeList::GetU32AttributeValue(int id, uint32_t *value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeU32)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeU32 exists.";
    return false;
  }
  const Nl80211U32Attribute *attr =
      reinterpret_cast<const Nl80211U32Attribute *>(GetAttribute(id));
  return attr->GetU32Value(value);
}

bool AttributeList::CreateU32Attribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = new Nl80211U32Attribute(id, id_string);
  return true;
}

bool AttributeList::SetU32AttributeValue(int id, uint32_t value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeU32)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeU32 exists.";
    return false;
  }
  Nl80211U32Attribute *attr =
      reinterpret_cast<Nl80211U32Attribute *>(GetAttribute(id));
  return attr->SetU32Value(value);
}

// U64 Attribute.

bool AttributeList::GetU64AttributeValue(int id, uint64_t *value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeU64)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeU64 exists.";
    return false;
  }
  const Nl80211U64Attribute *attr =
      reinterpret_cast<const Nl80211U64Attribute *>(GetAttribute(id));
  return attr->GetU64Value(value);
}

bool AttributeList::CreateU64Attribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = new Nl80211U64Attribute(id, id_string);
  return true;
}

bool AttributeList::SetU64AttributeValue(int id, uint64_t value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeU64)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeU64 exists.";
    return false;
  }
  Nl80211U64Attribute *attr =
      reinterpret_cast<Nl80211U64Attribute *>(GetAttribute(id));
  return attr->SetU64Value(value);
}

// Flag Attribute.

bool AttributeList::GetFlagAttributeValue(int id, bool *value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeFlag)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeFlag exists.";
    return false;
  }
  const Nl80211FlagAttribute *attr =
      reinterpret_cast<const Nl80211FlagAttribute *>(GetAttribute(id));
  return attr->GetFlagValue(value);
}

bool AttributeList::CreateFlagAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = new Nl80211FlagAttribute(id, id_string);
  return true;
}

bool AttributeList::SetFlagAttributeValue(int id, bool value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeFlag)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeFlag exists.";
    return false;
  }
  Nl80211FlagAttribute *attr =
      reinterpret_cast<Nl80211FlagAttribute *>(GetAttribute(id));
  return attr->SetFlagValue(value);
}

bool AttributeList::IsFlagAttributeTrue(int id) const {
  // TODO(wdg): After message constructors add attributes, remove the following
  // lines.
  if (!HasAttribute(id, Nl80211Attribute::kTypeFlag)) {
    return false;
  }

  bool flag;
  if (!GetFlagAttributeValue(id, &flag)) {
    return false;
  }
  return flag;
}

// String Attribute.

bool AttributeList::GetStringAttributeValue(int id, string *value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeString)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeString exists.";
    return false;
  }
  const Nl80211StringAttribute *attr =
      reinterpret_cast<const Nl80211StringAttribute *>(GetAttribute(id));
  return attr->GetStringValue(value);
}

bool AttributeList::CreateStringAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = new Nl80211StringAttribute(id, id_string);
  return true;
}

bool AttributeList::SetStringAttributeValue(int id, string value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeString)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeString exists.";
    return false;
  }
  Nl80211StringAttribute *attr =
      reinterpret_cast<Nl80211StringAttribute *>(GetAttribute(id));
  return attr->SetStringValue(value);
}

// Nested Attribute.

bool AttributeList::GetNestedAttributeValue(
    int id, WeakPtr<AttributeList> *value) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeNested)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeNested exists.";
    return false;
  }
  Nl80211NestedAttribute *attr =
      reinterpret_cast<Nl80211NestedAttribute *>(GetAttribute(id));
  return attr->GetNestedValue(value);
}

bool AttributeList::CreateNestedAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = new Nl80211NestedAttribute(id, id_string);
  return true;
}

// Raw Attribute.

bool AttributeList::GetRawAttributeValue(int id, ByteString *output) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeRaw)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeRaw exists.";
    return false;
  }
  const Nl80211RawAttribute *attr =
      reinterpret_cast<const Nl80211RawAttribute *>(GetAttribute(id));

  ByteString raw_value;

  if (!attr->GetRawValue(&raw_value))
    return false;

  if (output) {
    const nlattr *const_data =
        reinterpret_cast<const nlattr *>(raw_value.GetConstData());
    // nla_data and nla_len don't change their parameters but don't declare
    // them to be const.  Hence the cast.
    nlattr *data_nlattr = const_cast<nlattr *>(const_data);
    *output = ByteString(
        reinterpret_cast<unsigned char *>(nla_data(data_nlattr)),
        nla_len(data_nlattr));
  }
  return true;
}

const Nl80211RawAttribute *AttributeList::GetRawAttribute(int id) const {
  if (!HasAttribute(id, Nl80211Attribute::kTypeRaw)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeRaw exists.";
    return NULL;
  }
  const Nl80211RawAttribute *attr =
      reinterpret_cast<const Nl80211RawAttribute *>(GetAttribute(id));
  return attr;
}

Nl80211Attribute *AttributeList::GetAttribute(int id) const {
  map<int, Nl80211Attribute *>::const_iterator i;
  i = attributes_.find(id);
  if (i == attributes_.end()) {
    return NULL;
  }
  return i->second;
}

bool AttributeList::HasAttribute(int id,
                                 Nl80211Attribute::Type datatype) const {
  map<int, Nl80211Attribute *>::const_iterator i;
  i = attributes_.find(id);
  if (i == attributes_.end()) {
    LOG(ERROR) << "FALSE - Didn't find id " << id;
    return false;
  }
  return (i->second->datatype() == datatype) ? true : false;
}


}  // namespace shill
