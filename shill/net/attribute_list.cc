// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/attribute_list.h"

#include <ctype.h>
#include <linux/nl80211.h>
#include <netlink/attr.h>
#include <netlink/netlink.h>

#include <iomanip>
#include <map>
#include <string>

#include <base/logging.h>
#include <base/stl_util.h>
#include <base/strings/stringprintf.h>

#include "shill/net/netlink_attribute.h"
#include "shill/net/netlink_message.h"

using base::StringAppendF;
using base::WeakPtr;
using std::map;
using std::string;

namespace shill {

bool AttributeList::CreateAttribute(
    int id, AttributeList::NewFromIdMethod factory) {
  if (ContainsKey(attributes_, id)) {
    VLOG(7) << "Trying to re-add attribute " << id << ", not overwriting";
    return true;
  }
  attributes_[id] = AttributePointer(factory.Run(id));
  return true;
}

bool AttributeList::CreateControlAttribute(int id) {
  return CreateAttribute(
      id, base::Bind(&NetlinkAttribute::NewControlAttributeFromId));
}

bool AttributeList::CreateNl80211Attribute(
    int id, NetlinkMessage::MessageContext context) {
  return CreateAttribute(
      id, base::Bind(&NetlinkAttribute::NewNl80211AttributeFromId, context));
}

bool AttributeList::CreateAndInitAttribute(
    int id, const nlattr *data, AttributeList::NewFromIdMethod factory) {
  if (!CreateAttribute(id, factory)) {
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
  NetlinkAttribute *attribute = GetAttribute(id);
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
      new NetlinkU8Attribute(id, id_string));
  return true;
}

bool AttributeList::SetU8AttributeValue(int id, uint8_t value) {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetU8Value(value);
}


// U16 Attribute.

bool AttributeList::GetU16AttributeValue(int id, uint16_t *value) const {
  NetlinkAttribute *attribute = GetAttribute(id);
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
      new NetlinkU16Attribute(id, id_string));
  return true;
}

bool AttributeList::SetU16AttributeValue(int id, uint16_t value) {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetU16Value(value);
}

// U32 Attribute.

bool AttributeList::GetU32AttributeValue(int id, uint32_t *value) const {
  NetlinkAttribute *attribute = GetAttribute(id);
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
      new NetlinkU32Attribute(id, id_string));
  return true;
}

bool AttributeList::SetU32AttributeValue(int id, uint32_t value) {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetU32Value(value);
}

// U64 Attribute.

bool AttributeList::GetU64AttributeValue(int id, uint64_t *value) const {
  NetlinkAttribute *attribute = GetAttribute(id);
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
      new NetlinkU64Attribute(id, id_string));
  return true;
}

bool AttributeList::SetU64AttributeValue(int id, uint64_t value) {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetU64Value(value);
}

// Flag Attribute.

bool AttributeList::GetFlagAttributeValue(int id, bool *value) const {
  NetlinkAttribute *attribute = GetAttribute(id);
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
      new NetlinkFlagAttribute(id, id_string));
  return true;
}

bool AttributeList::SetFlagAttributeValue(int id, bool value) {
  NetlinkAttribute *attribute = GetAttribute(id);
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
  NetlinkAttribute *attribute = GetAttribute(id);
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
      new NetlinkStringAttribute(id, id_string));
  return true;
}

bool AttributeList::CreateSsidAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new NetlinkSsidAttribute(id, id_string));
  return true;
}

bool AttributeList::SetStringAttributeValue(int id, string value) {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetStringValue(value);
}

// Nested Attribute.

bool AttributeList::GetNestedAttributeList(int id,
                                           AttributeListRefPtr *value) {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->GetNestedAttributeList(value);
}

bool AttributeList::ConstGetNestedAttributeList(
    int id, AttributeListConstRefPtr *value) const {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->ConstGetNestedAttributeList(value);
}

bool AttributeList::SetNestedAttributeHasAValue(int id) {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetNestedHasAValue();
}

bool AttributeList::CreateNestedAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(
      new NetlinkNestedAttribute(id, id_string));
  return true;
}

// Raw Attribute.

bool AttributeList::GetRawAttributeValue(int id,
                                         ByteString *output) const {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;

  ByteString raw_value;

  if (!attribute->GetRawValue(&raw_value))
    return false;

  if (output) {
    *output = raw_value;
  }
  return true;
}

bool AttributeList::SetRawAttributeValue(int id, ByteString value) {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;
  return attribute->SetRawValue(value);
}

bool AttributeList::CreateRawAttribute(int id, const char *id_string) {
  if (ContainsKey(attributes_, id)) {
    LOG(ERROR) << "Trying to re-add attribute: " << id;
    return false;
  }
  attributes_[id] = AttributePointer(new NetlinkRawAttribute(id, id_string));
  return true;
}

bool AttributeList::GetAttributeAsString(int id, std::string *value) const {
  NetlinkAttribute *attribute = GetAttribute(id);
  if (!attribute)
    return false;

  return attribute->ToString(value);
}

NetlinkAttribute *AttributeList::GetAttribute(int id) const {
  map<int, AttributePointer>::const_iterator i;
  i = attributes_.find(id);
  if (i == attributes_.end()) {
    return nullptr;
  }
  return i->second.get();
}

}  // namespace shill
