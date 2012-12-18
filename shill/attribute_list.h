// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ATTRIBUTE_LIST_H_
#define SHILL_ATTRIBUTE_LIST_H_

#include <linux/nl80211.h>
#include <netlink/netlink.h>

#include <map>
#include <string>

#include "shill/nl80211_attribute.h"

struct nlattr;
namespace shill {

class Nl80211RawAttribute;

class AttributeList {
 public:
  ~AttributeList();

  // Instantiates an Nl80211Attribute of the appropriate type from |name|,
  // and adds it to |attributes_|.
  bool CreateAttribute(nl80211_attrs name);

  // Instantiates an Nl80211Attribute of the appropriate type from |name|,
  // initializes it from |data|, and adds it to |attributes_|.
  // TODO(wdg): This is a stop-gap for use before message constructors add
  // their attributes as message templates.
  bool CreateAndInitFromNlAttr(nl80211_attrs name, const nlattr *data);

  bool GetU8AttributeValue(nl80211_attrs name, uint8_t *value) const;
  bool GetU16AttributeValue(nl80211_attrs name, uint16_t *value) const;
  bool GetU32AttributeValue(nl80211_attrs name, uint32_t *value) const;
  bool GetU64AttributeValue(nl80211_attrs name, uint64_t *value) const;
  bool GetFlagAttributeValue(nl80211_attrs name, bool *value) const;
  // |IsFlagAttributeTrue| returns true if the flag attribute named |name| is
  // true.  It retruns false if the attribute does not exist, is not of type
  // kTypeFlag, or is not true.
  bool IsFlagAttributeTrue(nl80211_attrs name) const;
  bool GetStringAttributeValue(nl80211_attrs name, std::string *value) const;

  // A raw attribute is a place to store unrecognized attributes when they
  // from the kernel.  For this reason, only limited support is provided for
  // them.
  bool GetRawAttributeValue(nl80211_attrs name, ByteString *output) const;
  // TODO(wdg): |GetRawAttribute| is a stopgap to support various
  // UserBoundNlMessage::ToString methods and must, once those are re-written,
  // be destroyed.
  const Nl80211RawAttribute *GetRawAttribute(nl80211_attrs name) const;

 private:
  // Using this to get around issues with const and operator[].
  Nl80211Attribute *GetAttribute(nl80211_attrs name) const;

  bool HasAttribute(nl80211_attrs name, Nl80211Attribute::Type type) const;

  std::map<int, Nl80211Attribute *> attributes_;
};

}  // namespace shill

#endif  // SHILL_ATTRIBUTE_LIST_H_

