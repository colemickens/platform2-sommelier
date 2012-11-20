// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NLATTRIBUTE_H_
#define SHILL_NLATTRIBUTE_H_

#include <linux/nl80211.h>
#include <netlink/netlink.h>

#include <map>
#include <string>

#include <base/memory/scoped_ptr.h>

#include "shill/byte_string.h"

struct nlattr;

namespace shill {

// Nl80211Attribute is an abstract base class that describes an attribute in a
// netlink-80211 message.  Child classes are type-specific and will define
// Get*Value and Set*Value methods (where * is the type).  A second-level of
// child classes exist for each individual attribute type.
//
// An attribute has a name (which is really an enumerated value), a data type,
// and a value.  In an nlattr (the underlying format for an attribute in a
// message), the data is stored as a blob without type information; the writer
// and reader of the attribute must agree on the data type.
class Nl80211Attribute {
 public:
  enum Type {
    kTypeU8,
    kTypeU16,
    kTypeU32,
    kTypeU64,
    kTypeString,
    kTypeFlag,
    kTypeMsecs,
    kTypeNested,
    kTypeRaw,
    kTypeError
  };

  Nl80211Attribute(nl80211_attrs name, const char *name_string,
                   Type type, const char *type_string);
  virtual ~Nl80211Attribute() {}

  virtual bool InitFromNlAttr(const nlattr *data);

  // Static factory generates the appropriate Nl80211Attribute object from the
  // raw nlattr data.
  static Nl80211Attribute *NewFromNlAttr(nl80211_attrs name,
                                         const nlattr *data);

  // Accessors for the attribute's name and type information.
  nl80211_attrs name() const { return name_; }
  virtual const char *name_string() const { return name_string_; }
  Type type() const { return type_; }
  std::string type_string() const { return type_string_; }

  // TODO(wdg): Since |data| is used, externally, to support |nla_parse_nested|,
  // make it protected once all functionality has been brought inside the
  // Nl80211Attribute classes.
  //
  // |data_| contains an 'nlattr *' but it's been stored as a ByteString.
  // This returns a pointer to the data in the form that is intended.
  const nlattr *data() const {
    return reinterpret_cast<const nlattr *>(data_.GetConstData());
  }

  // TODO(wdg): GetRawData is used to support
  // UserBoundNlMessage::GetRawAttributeData which, in turn, is used to support
  // NL80211_ATTR_FRAME and NL80211_ATTR_KEY_SEQ.  Remove this method (and that
  // one) once those classes are fleshed-out.
  //
  // If successful, returns 'true' and sets *|value| to the raw attribute data
  // (after the header) for this attribute.  Otherwise, returns 'false' and
  // leaves |value| unchanged.
  bool GetRawData(ByteString *value) const;

  // Fill a string with characters that represents the value of the attribute.
  // If no attribute is found or if the type isn't trivially stringizable,
  // this method returns 'false' and |value| remains unchanged.
  virtual bool AsString(std::string *value) const = 0;

  // Writes the raw attribute data to a string.  For debug.
  std::string RawToString() const;

 protected:
  // Raw data corresponding to the value in any of the child classes.
  // TODO(wdg): When 'data()' is removed, move this to the Nl80211RawAttribute
  // class.
  ByteString data_;

 private:
  nl80211_attrs name_;
  const char *name_string_;
  Type type_;
  const char *type_string_;
};

// Type-specific sub-classes.  These provide their own type-specific data get
// and set functions.

class Nl80211U32Attribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211U32Attribute(nl80211_attrs name, const char *name_string)
      : Nl80211Attribute(name, name_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU32Value(uint32_t *value) const;
  bool SetU32Value(uint32_t new_value);
  bool AsString(std::string *value) const;

 private:
  uint32_t value_;
};

// TODO(wdg): Add more data types.

class Nl80211RawAttribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211RawAttribute(nl80211_attrs name, const char *name_string)
      : Nl80211Attribute(name, name_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetRawValue(const ByteString **value) const;
  // Not supporting 'set' for raw data.  This type is a "don't know" type to
  // be used for user-bound massages (via InitFromNlAttr).  The 'set' method
  // is intended for building kernel-bound messages and shouldn't be used with
  // raw data.
  bool AsString(std::string *value) const;
};

// Attribute-specific sub-classes.

class Nl80211AttributeDuration : public Nl80211U32Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  explicit Nl80211AttributeDuration()
      : Nl80211U32Attribute(kName, kNameString) {}
};

class Nl80211AttributeGeneric : public Nl80211RawAttribute {
 public:
  explicit Nl80211AttributeGeneric(nl80211_attrs name);
  const char *name_string() const;

 private:
  std::string name_string_;
};

}  // namespace shill

#endif  // SHILL_NLATTRIBUTE_H_
