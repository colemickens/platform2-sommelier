// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NLATTRIBUTE_H_
#define SHILL_NLATTRIBUTE_H_

#include <linux/nl80211.h>
#include <netlink/attr.h>
#include <netlink/netlink.h>

#include <map>
#include <string>

#include <base/memory/scoped_ptr.h>

#include "shill/byte_string.h"

struct nlattr;

namespace shill {

class AttributeList;

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
    kTypeFlag,
    kTypeString,
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
  static Nl80211Attribute *NewFromName(nl80211_attrs name);

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

  // Fill a string with characters that represents the value of the attribute.
  // If no attribute is found or if the type isn't trivially stringizable,
  // this method returns 'false' and |value| remains unchanged.
  virtual bool AsString(std::string *value) const = 0;

  // Writes the raw attribute data to a string.  For debug.
  std::string RawToString() const;

  // Note that |nla_get_*| don't change their arguments but don't declare
  // themselves as 'const', either.  These methods wrap the const castness.
  static char *NlaGetString(const nlattr *input) {
    return nla_get_string(const_cast<nlattr *>(input));
  }
  static uint8_t NlaGetU8(const nlattr *input) {
    return nla_get_u8(const_cast<nlattr *>(input));
  }
  static uint16_t NlaGetU16(const nlattr *input) {
    return nla_get_u16(const_cast<nlattr *>(input));
  }
  static uint32_t NlaGetU32(const nlattr *input) {
    return nla_get_u32(const_cast<nlattr *>(input));
  }
  static uint64_t NlaGetU64(const nlattr *input) {
    return nla_get_u64(const_cast<nlattr *>(input));
  }

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

// U8.

class Nl80211U8Attribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211U8Attribute(nl80211_attrs name, const char *name_string)
      : Nl80211Attribute(name, name_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU8Value(uint8_t *value) const;
  bool SetU8Value(uint8_t new_value);
  bool AsString(std::string *value) const;

 private:
  uint8_t value_;
};

class Nl80211AttributeKeyIdx : public Nl80211U8Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeKeyIdx() : Nl80211U8Attribute(kName, kNameString) {}
};

class Nl80211AttributeRegType : public Nl80211U8Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeRegType() : Nl80211U8Attribute(kName, kNameString) {}
};

// U16.

class Nl80211U16Attribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211U16Attribute(nl80211_attrs name, const char *name_string)
      : Nl80211Attribute(name, name_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU16Value(uint16_t *value) const;
  bool SetU16Value(uint16_t new_value);
  bool AsString(std::string *value) const;

 private:
  uint16_t value_;
};

class Nl80211AttributeReasonCode : public Nl80211U16Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeReasonCode() : Nl80211U16Attribute(kName, kNameString) {}
};

class Nl80211AttributeStatusCode : public Nl80211U16Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeStatusCode() : Nl80211U16Attribute(kName, kNameString) {}
};

// U32.

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

class Nl80211AttributeDuration : public Nl80211U32Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeDuration() : Nl80211U32Attribute(kName, kNameString) {}
};

class Nl80211AttributeGeneration : public Nl80211U32Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeGeneration() : Nl80211U32Attribute(kName, kNameString) {}
};

class Nl80211AttributeIfindex : public Nl80211U32Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeIfindex() : Nl80211U32Attribute(kName, kNameString) {}
};

class Nl80211AttributeKeyType : public Nl80211U32Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeKeyType() : Nl80211U32Attribute(kName, kNameString) {}
};

class Nl80211AttributeRegInitiator : public Nl80211U32Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeRegInitiator() : Nl80211U32Attribute(kName, kNameString) {}
};

class Nl80211AttributeWiphy : public Nl80211U32Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeWiphy() : Nl80211U32Attribute(kName, kNameString) {}
};

class Nl80211AttributeWiphyFreq : public Nl80211U32Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeWiphyFreq() : Nl80211U32Attribute(kName, kNameString) {}
};

// U64.

class Nl80211U64Attribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211U64Attribute(nl80211_attrs name, const char *name_string)
      : Nl80211Attribute(name, name_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU64Value(uint64_t *value) const;
  bool SetU64Value(uint64_t new_value);
  bool AsString(std::string *value) const;

 private:
  uint64_t value_;
};

class Nl80211AttributeCookie : public Nl80211U64Attribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeCookie() : Nl80211U64Attribute(kName, kNameString) {}
};

// Flag.

class Nl80211FlagAttribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211FlagAttribute(nl80211_attrs name, const char *name_string)
      : Nl80211Attribute(name, name_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetFlagValue(bool *value) const;
  bool SetFlagValue(bool new_value);
  bool AsString(std::string *value) const;

 private:
  bool value_;
};

class Nl80211AttributeDisconnectedByAp : public Nl80211FlagAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeDisconnectedByAp() :
    Nl80211FlagAttribute(kName, kNameString) {}
};

class Nl80211AttributeSupportMeshAuth : public Nl80211FlagAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeSupportMeshAuth() :
    Nl80211FlagAttribute(kName, kNameString) {}
};

class Nl80211AttributeTimedOut : public Nl80211FlagAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeTimedOut() : Nl80211FlagAttribute(kName, kNameString) {}
};

// String.

class Nl80211StringAttribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211StringAttribute(nl80211_attrs name, const char *name_string)
      : Nl80211Attribute(name, name_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetStringValue(std::string *value) const;
  bool SetStringValue(const std::string new_value);
  bool AsString(std::string *value) const;

 private:
  std::string value_;
};

class Nl80211AttributeRegAlpha2 : public Nl80211StringAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeRegAlpha2() : Nl80211StringAttribute(kName, kNameString) {}
};

class Nl80211AttributeWiphyName : public Nl80211StringAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeWiphyName() : Nl80211StringAttribute(kName, kNameString) {}
};

// Raw.

class Nl80211RawAttribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211RawAttribute(nl80211_attrs name, const char *name_string)
      : Nl80211Attribute(name, name_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetRawValue(ByteString *value) const;
  // Not supporting 'set' for raw data.  This type is a "don't know" type to
  // be used for user-bound massages (via InitFromNlAttr).  The 'set' method
  // is intended for building kernel-bound messages and shouldn't be used with
  // raw data.
  bool AsString(std::string *value) const;
};

// TODO(wdg): This should inherit from Nl80211NestedAttribute when that class
// exists.
class Nl80211AttributeCqm : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeCqm() : Nl80211RawAttribute(kName, kNameString) {}
};

class Nl80211AttributeFrame : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeFrame() : Nl80211RawAttribute(kName, kNameString) {}
};

class Nl80211AttributeGeneric : public Nl80211RawAttribute {
 public:
  explicit Nl80211AttributeGeneric(nl80211_attrs name);
  const char *name_string() const;

 private:
  std::string name_string_;
};

class Nl80211AttributeKeySeq : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeKeySeq() : Nl80211RawAttribute(kName, kNameString) {}
};

class Nl80211AttributeMac : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeMac() : Nl80211RawAttribute(kName, kNameString) {}
};

class Nl80211AttributeRespIe : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeRespIe() : Nl80211RawAttribute(kName, kNameString) {}
};

class Nl80211AttributeScanFrequencies : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeScanFrequencies() : Nl80211RawAttribute(kName, kNameString) {}
};

class Nl80211AttributeScanSsids : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeScanSsids() : Nl80211RawAttribute(kName, kNameString) {}
};

class Nl80211AttributeStaInfo : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeStaInfo() : Nl80211RawAttribute(kName, kNameString) {}
};

}  // namespace shill

#endif  // SHILL_NLATTRIBUTE_H_
