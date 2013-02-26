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
#include <base/stringprintf.h>

#include "shill/logging.h"
#include "shill/nl80211_attribute.h"
#include "shill/scope_logger.h"

using base::StringAppendF;
using base::WeakPtr;
using std::map;
using std::string;

namespace shill {

bool AttributeList::CreateAttribute(nl80211_attrs id) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(Nl80211Attribute::NewFromName(id));
  return true;
}

bool AttributeList::CreateAndInitFromNlAttr(nl80211_attrs id,
                                            const nlattr *data) {
  if (!CreateAttribute(id)) {
    return false;
  }
  return attributes_[id]->InitFromNlAttr(data);
}

void AttributeList::Print(int log_level, int indent) const {
  map<int, AttributePointer>::const_iterator i;

  for (i = attributes_.begin(); i != attributes_.end(); ++i) {
    i->second->Print(log_level, indent);
  }
}

ByteString AttributeList::Encode() const {
  ByteString result;
  map<int, AttributePointer>::const_iterator i;

  for (i = attributes_.begin(); i != attributes_.end(); ++i) {
    result.Append(i->second->Encode());
  }
  return result;
}

// U8 Attribute.

bool AttributeList::GetU8AttributeValue(int id, uint8_t *value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->GetU8Value(value);
}

bool AttributeList::CreateU8Attribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new Nl80211U8Attribute(id, id_string));
  return true;
}

bool AttributeList::SetU8AttributeValue(int id, uint8_t value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetU8Value(value);
}


// U16 Attribute.

bool AttributeList::GetU16AttributeValue(int id, uint16_t *value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->GetU16Value(value);
}

bool AttributeList::CreateU16Attribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new Nl80211U16Attribute(id, id_string));
  return true;
}

bool AttributeList::SetU16AttributeValue(int id, uint16_t value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetU16Value(value);
}

// U32 Attribute.

bool AttributeList::GetU32AttributeValue(int id, uint32_t *value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->GetU32Value(value);
}

bool AttributeList::CreateU32Attribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new Nl80211U32Attribute(id, id_string));
  return true;
}

bool AttributeList::SetU32AttributeValue(int id, uint32_t value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetU32Value(value);
}

// U64 Attribute.

bool AttributeList::GetU64AttributeValue(int id, uint64_t *value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->GetU64Value(value);
}

bool AttributeList::CreateU64Attribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new Nl80211U64Attribute(id, id_string));
  return true;
}

bool AttributeList::SetU64AttributeValue(int id, uint64_t value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetU64Value(value);
}

// Flag Attribute.

bool AttributeList::GetFlagAttributeValue(int id, bool *value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->GetFlagValue(value);
}

bool AttributeList::CreateFlagAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new Nl80211FlagAttribute(id, id_string));
  return true;
}

bool AttributeList::SetFlagAttributeValue(int id, bool value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetFlagValue(value);
}

bool AttributeList::IsFlagAttributeTrue(int id) const {
  bool flag;
  if (!GetFlagAttributeValue(id, &flag)) {
    return false;
  }
  return flag;
}

// String Attribute.

bool AttributeList::GetStringAttributeValue(int id, string *value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->GetStringValue(value);
}

bool AttributeList::CreateStringAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new Nl80211StringAttribute(id, id_string));
  return true;
}

bool AttributeList::SetStringAttributeValue(int id, string value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetStringValue(value);
}

// Nested Attribute.

bool AttributeList::GetNestedAttributeValue(
    int id, WeakPtr<AttributeList> *value) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->GetNestedValue(value);
}

bool AttributeList::CreateNestedAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new Nl80211NestedAttribute(id, id_string));
  return true;
}

// Raw Attribute.

bool AttributeList::GetRawAttributeValue(int id,
                                         ByteString *output) const {
  Nl80211Attribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;

  ByteString raw_value;

  if (!attribute->GetRawValue(&raw_value))
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

const Nl80211RawAttribute *AttributeList::GetRawAttribute(
    int id) const {
  if (!HasRawAttribute(id)) {
    LOG(ERROR) << "No attribute " << id << " of type kTypeRaw exists.";
    return NULL;
  }
  const Nl80211RawAttribute *attr =
      reinterpret_cast<const Nl80211RawAttribute *>(GetAttribute(id));
  return attr;
}

Nl80211Attribute *AttributeList::GetAttribute(int id) const {
  map<int, AttributePointer>::const_iterator i;
  i = attributes_.find(id);
  if (i == attributes_.end()) {
    return NULL;
  }
  return i->second.get();
}

bool AttributeList::HasRawAttribute(int id) const {
  map<int, AttributePointer>::const_iterator i;
  i = attributes_.find(id);
  if (i == attributes_.end()) {
    LOG(ERROR) << "FALSE - Didn't find id " << id;
    return false;
  }
  return (i->second->datatype() == Nl80211Attribute::kTypeRaw) ? true : false;
}


}  // namespace shill
