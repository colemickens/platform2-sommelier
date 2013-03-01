// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ATTRIBUTE_LIST_H_
#define SHILL_ATTRIBUTE_LIST_H_

#include <linux/nl80211.h>
#include <netlink/netlink.h>

#include <map>
#include <string>
#include <tr1/memory>

#include <base/bind.h>

#include "shill/refptr_types.h"

struct nlattr;
namespace shill {

class ByteString;
class NetlinkAttribute;
class NetlinkRawAttribute;

class AttributeList : public base::RefCounted<AttributeList> {
 public:
  typedef std::tr1::shared_ptr<NetlinkAttribute> AttributePointer;
  typedef base::Callback<NetlinkAttribute *(int id)> NewFromIdMethod;

  // Instantiates an NetlinkAttribute of the appropriate type from |id|,
  // and adds it to |attributes_|.
  bool CreateAttribute(int id, NewFromIdMethod factory);

  // Instantiates an NetlinkAttribute of the appropriate type from |id|,
  // initializes it from |data|, and adds it to |attributes_|.
  // TODO(wdg): This is a stop-gap for use before message constructors add
  // their attributes as message templates.
  bool CreateAndInitAttribute(int id, const nlattr *data,
                              AttributeList::NewFromIdMethod factory);

  // Prints the attribute list with each attribute using no less than 1 line.
  // |indent| indicates the amout of leading spaces to be printed (useful for
  // nested attributes).
  void Print(int log_level, int indent) const;

  // Returns the attributes as the payload portion of a netlink message
  // suitable for Sockets::Send.  Return value is empty on failure (or if no
  // attributes exist).
  ByteString Encode() const;

  bool CreateU8Attribute(int id, const char *id_string);
  bool SetU8AttributeValue(int id, uint8_t value);
  bool GetU8AttributeValue(int id, uint8_t *value) const;

  bool CreateU16Attribute(int id, const char *id_string);
  bool SetU16AttributeValue(int id, uint16_t value);
  bool GetU16AttributeValue(int id, uint16_t *value) const;

  bool CreateU32Attribute(int id, const char *id_string);
  bool SetU32AttributeValue(int id, uint32_t value);
  bool GetU32AttributeValue(int id, uint32_t *value) const;

  bool CreateU64Attribute(int id, const char *id_string);
  bool SetU64AttributeValue(int id, uint64_t value);
  bool GetU64AttributeValue(int id, uint64_t *value) const;

  bool CreateFlagAttribute(int id, const char *id_string);
  bool SetFlagAttributeValue(int id, bool value);
  bool GetFlagAttributeValue(int id, bool *value) const;
  // |IsFlagAttributeTrue| returns true if the flag attribute |id| is true.  It
  // retruns false if the attribute does not exist, is not of type kTypeFlag,
  // or is not true.
  bool IsFlagAttributeTrue(int id) const;

  bool CreateStringAttribute(int id, const char *id_string);
  bool SetStringAttributeValue(int id, std::string value);
  bool GetStringAttributeValue(int id, std::string *value) const;

  bool CreateNestedAttribute(int id, const char *id_string);
  bool SetNestedAttributeHasAValue(int id);
  bool GetNestedAttributeList(int id, AttributeListRefPtr *value);
  bool ConstGetNestedAttributeList(int id,
                                   AttributeListConstRefPtr *value) const;

  // A raw attribute is a place to store unrecognized attributes when they
  // from the kernel.  For this reason, only limited support is provided for
  // them.
  bool GetRawAttributeValue(int id, ByteString *output) const;
  // TODO(wdg): |GetRawAttribute| is a stopgap to support various
  // Nl80211Message::ToString methods and must, once those are re-written,
  // be destroyed.
  const NetlinkRawAttribute *GetRawAttribute(int id) const;

 private:
  friend class NetlinkNestedAttribute;

  // Using this to get around issues with const and operator[].
  NetlinkAttribute *GetAttribute(int id) const;

  // TODO(wdg): This is only used to support |GetRawAttribute|.  Delete this
  // when that goes away.
  bool HasRawAttribute(int id) const;

  std::map<int, AttributePointer> attributes_;
};

}  // namespace shill

#endif  // SHILL_ATTRIBUTE_LIST_H_
