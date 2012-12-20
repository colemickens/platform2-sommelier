// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ATTRIBUTE_LIST_H_
#define SHILL_ATTRIBUTE_LIST_H_

#include <linux/nl80211.h>
#include <netlink/netlink.h>

#include <map>
#include <string>

#include <base/memory/weak_ptr.h>
#include "shill/nl80211_attribute.h"

struct nlattr;
namespace shill {

class ByteString;
class Nl80211Attribute;
class Nl80211RawAttribute;

class AttributeList : public base::SupportsWeakPtr<AttributeList> {
 public:
  virtual ~AttributeList();

  // Instantiates an Nl80211Attribute of the appropriate type from |id|,
  // and adds it to |attributes_|.
  bool CreateAttribute(nl80211_attrs id);

  // Instantiates an Nl80211Attribute of the appropriate type from |id|,
  // initializes it from |data|, and adds it to |attributes_|.
  // TODO(wdg): This is a stop-gap for use before message constructors add
  // their attributes as message templates.
  bool CreateAndInitFromNlAttr(nl80211_attrs id, const nlattr *data);

  bool GetU8AttributeValue(int id, uint8_t *value) const;
  bool GetU16AttributeValue(int id, uint16_t *value) const;
  bool GetU32AttributeValue(int id, uint32_t *value) const;
  bool GetU64AttributeValue(int id, uint64_t *value) const;
  bool GetFlagAttributeValue(int id, bool *value) const;
  // |IsFlagAttributeTrue| returns true if the flag attribute |id| is true.  It
  // retruns false if the attribute does not exist, is not of type kTypeFlag,
  // or is not true.
  bool IsFlagAttributeTrue(int id) const;
  bool GetStringAttributeValue(int id, std::string *value) const;
  bool GetNestedAttributeValue(int id,
                               base::WeakPtr<AttributeList> *value) const;

  // A raw attribute is a place to store unrecognized attributes when they
  // from the kernel.  For this reason, only limited support is provided for
  // them.
  bool GetRawAttributeValue(int id, ByteString *output) const;
  // TODO(wdg): |GetRawAttribute| is a stopgap to support various
  // UserBoundNlMessage::ToString methods and must, once those are re-written,
  // be destroyed.
  const Nl80211RawAttribute *GetRawAttribute(int id) const;

 private:
  // The Create*Attribute and Set*Attribute methods are specifically for use
  // by nested attributes to add their sub-attributes.  Classes derived from
  // Nl80211NestedAttribute should be added, here.
  friend class Nl80211AttributeCqm;
  friend class Nl80211AttributeStaInfo;

  bool CreateU8Attribute(int id, const char *id_string);
  bool SetU8AttributeValue(int id, uint8_t value) const;

  bool CreateU16Attribute(int id, const char *id_string);
  bool SetU16AttributeValue(int id, uint16_t value) const;

  bool CreateU32Attribute(int id, const char *id_string);
  bool SetU32AttributeValue(int id, uint32_t value) const;

  bool CreateU64Attribute(int id, const char *id_string);
  bool SetU64AttributeValue(int id, uint64_t value) const;

  bool CreateFlagAttribute(int id, const char *id_string);
  bool SetFlagAttributeValue(int id, bool value) const;

  bool CreateStringAttribute(int id, const char *id_string);
  bool SetStringAttributeValue(int id, std::string value) const;

  bool CreateNestedAttribute(int id, const char *id_string);
  // No |SetNestedAttributeValue| method as it would make no sense.

  // Using this to get around issues with const and operator[].
  Nl80211Attribute *GetAttribute(int id) const;

  bool HasAttribute(int name, Nl80211Attribute::Type type) const;

  std::map<int, Nl80211Attribute *> attributes_;
};

}  // namespace shill

#endif  // SHILL_ATTRIBUTE_LIST_H_
