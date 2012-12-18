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

using std::string;
using std::map;

namespace shill {

AttributeList::~AttributeList() {
  map<int, Nl80211Attribute *>::iterator i;
  for (i = attributes_.begin(); i != attributes_.end(); ++i) {
    delete i->second;
  }
}

bool AttributeList::CreateAttribute(nl80211_attrs name) {
  if (ContainsKey(attributes_, name)) {
    LOG(ERROR) << "Trying to re-add attribute: " << name;
    return false;
  }
  attributes_[name] = Nl80211Attribute::NewFromName(name);
  return true;
}

bool AttributeList::CreateAndInitFromNlAttr(nl80211_attrs name,
                                            const nlattr *data) {
  if (!CreateAttribute(name)) {
    return false;
  }
  return attributes_[name]->InitFromNlAttr(data);
}

// U8 Attribute.

bool AttributeList::GetU8AttributeValue(nl80211_attrs name,
                                        uint8_t *value) const {
  CHECK(HasAttribute(name, Nl80211Attribute::kTypeU8));
  const Nl80211U8Attribute *attr =
      reinterpret_cast<const Nl80211U8Attribute *>(GetAttribute(name));
  return attr->GetU8Value(value);
}

// U16 Attribute.

bool AttributeList::GetU16AttributeValue(nl80211_attrs name,
                                         uint16_t *value) const {
  CHECK(HasAttribute(name, Nl80211Attribute::kTypeU16));
  const Nl80211U16Attribute *attr =
      reinterpret_cast<const Nl80211U16Attribute *>(GetAttribute(name));
  return attr->GetU16Value(value);
}

// U32 Attribute.

bool AttributeList::GetU32AttributeValue(nl80211_attrs name,
                                         uint32_t *value) const {
  CHECK(HasAttribute(name, Nl80211Attribute::kTypeU32));
  const Nl80211U32Attribute *attr =
      reinterpret_cast<const Nl80211U32Attribute *>(GetAttribute(name));
  return attr->GetU32Value(value);
}

// U64 Attribute.

bool AttributeList::GetU64AttributeValue(nl80211_attrs name,
                                         uint64_t *value) const {
  CHECK(HasAttribute(name, Nl80211Attribute::kTypeU64));
  const Nl80211U64Attribute *attr =
      reinterpret_cast<const Nl80211U64Attribute *>(GetAttribute(name));
  return attr->GetU64Value(value);
}

// Flag Attribute.

bool AttributeList::GetFlagAttributeValue(nl80211_attrs name,
                                          bool *value) const {
  CHECK(HasAttribute(name, Nl80211Attribute::kTypeFlag));
  const Nl80211FlagAttribute *attr =
      reinterpret_cast<const Nl80211FlagAttribute *>(GetAttribute(name));
  return attr->GetFlagValue(value);
}

bool AttributeList::IsFlagAttributeTrue(nl80211_attrs name) const {
  bool flag;

  // TODO(wdg): After message constructors add attributes, remove the following
  // lines.
  if (!HasAttribute(name, Nl80211Attribute::kTypeFlag)) {
    return false;
  }

  if (!GetFlagAttributeValue(name, &flag)) {
    return false;
  }
  return flag;
}

// String Attribute.

bool AttributeList::GetStringAttributeValue(nl80211_attrs name,
                                            string *value) const {
  CHECK(HasAttribute(name, Nl80211Attribute::kTypeString));
  const Nl80211StringAttribute *attr =
      reinterpret_cast<const Nl80211StringAttribute *>(GetAttribute(name));
  return attr->GetStringValue(value);
}

// Raw Attribute.

bool AttributeList::GetRawAttributeValue(nl80211_attrs name,
                                         ByteString *output) const {
  CHECK(HasAttribute(name, Nl80211Attribute::kTypeRaw));
  const Nl80211RawAttribute *attr =
      reinterpret_cast<const Nl80211RawAttribute *>(GetAttribute(name));

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

const Nl80211RawAttribute *AttributeList::GetRawAttribute(
    nl80211_attrs name) const {
  CHECK(HasAttribute(name, Nl80211Attribute::kTypeRaw));
  const Nl80211RawAttribute *attr =
      reinterpret_cast<const Nl80211RawAttribute *>(GetAttribute(name));
  return attr;
}

Nl80211Attribute *AttributeList::GetAttribute(nl80211_attrs name) const {
  map<int, Nl80211Attribute *>::const_iterator i;
  i = attributes_.find(name);
  if (i == attributes_.end()) {
    return NULL;
  }
  return i->second;
}

bool AttributeList::HasAttribute(nl80211_attrs name,
                                 Nl80211Attribute::Type type) const {
  map<int, Nl80211Attribute *>::const_iterator i;
  i = attributes_.find(name);
  if (i == attributes_.end()) {
    LOG(ERROR) << "FALSE - Didn't find name " << name;
    return false;
  }
  return (i->second->type() == type) ? true : false;
}


}  // namespace shill
