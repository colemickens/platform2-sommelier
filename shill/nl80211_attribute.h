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

#include <base/memory/weak_ptr.h>

#include "shill/attribute_list.h"
#include "shill/byte_string.h"
#include "shill/logging.h"

struct nlattr;

namespace shill {

// Nl80211Attribute is an abstract base class that describes an attribute in a
// netlink-80211 message.  Child classes are type-specific and will define
// Get*Value and Set*Value methods (where * is the type).  A second-level of
// child classes exist for each individual attribute type.
//
// An attribute has an id (which is really an enumerated value), a data type,
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

  Nl80211Attribute(int id, const char *id_string,
                   Type datatype, const char *datatype_string);
  virtual ~Nl80211Attribute() {}

  virtual bool InitFromNlAttr(const nlattr *data);

  // Static factory generates the appropriate Nl80211Attribute object from the
  // raw nlattr data.
  static Nl80211Attribute *NewFromName(nl80211_attrs id);

  // Accessors for the attribute's id and datatype information.
  int id() const { return id_; }
  virtual const char *id_string() const { return id_string_; }
  Type datatype() const { return datatype_; }
  const char *datatype_string() const { return datatype_string_; }

  // TODO(wdg): Since |data| is used, externally, to support |nla_parse_nested|,
  // make it protected once all functionality has been brought inside the
  // Nl80211Attribute classes.
  //
  // |data_| contains an 'nlattr *' but it's been stored as a ByteString.
  // This returns a pointer to the data in the form that is intended.
  const nlattr *data() const {
    return reinterpret_cast<const nlattr *>(data_.GetConstData());
  }

  virtual bool GetU8Value(uint8_t *value) const;
  virtual bool SetU8Value(uint8_t new_value);

  virtual bool GetU16Value(uint16_t *value) const;
  virtual bool SetU16Value(uint16_t value);

  virtual bool GetU32Value(uint32_t *value) const;
  virtual bool SetU32Value(uint32_t value);

  virtual bool GetU64Value(uint64_t *value) const;
  virtual bool SetU64Value(uint64_t value);

  virtual bool GetFlagValue(bool *value) const;
  virtual bool SetFlagValue(bool value);

  virtual bool GetStringValue(std::string *value) const;
  virtual bool SetStringValue(const std::string value);

  virtual bool GetNestedValue(base::WeakPtr<AttributeList> *value);

  virtual bool GetRawValue(ByteString *value) const;

  // Fill a string with characters that represents the value of the attribute.
  // If no attribute is found or if the datatype isn't trivially stringizable,
  // this method returns 'false' and |value| remains unchanged.
  virtual bool ToString(std::string *value) const = 0;

  // Writes the raw attribute data to a string.  For debug.
  std::string RawToString() const;

  // Encodes the attribute suitably for the attributes in the payload portion
  // of a netlink message suitable for Sockets::Send.  Return value is empty on
  // failure.
  virtual ByteString Encode() const = 0;

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
  // Encodes the attribute suitably for the attributes in the payload portion
  // of a netlink message suitable for Sockets::Send.  Return value is empty on
  // failure.
  virtual ByteString EncodeGeneric(const unsigned char *data, int bytes) const;

  // Raw data corresponding to the value in any of the child classes.
  // TODO(wdg): When 'data()' is removed, move this to the Nl80211RawAttribute
  // class.
  ByteString data_;

 private:
  int id_;  // In the non-nested case, this is really type nl80211_attrs.
  const char *id_string_;
  Type datatype_;
  const char *datatype_string_;
};

// U8.

class Nl80211U8Attribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211U8Attribute(int id, const char *id_string)
      : Nl80211Attribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU8Value(uint8_t *value) const;
  bool SetU8Value(uint8_t new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

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
  Nl80211U16Attribute(int id, const char *id_string)
      : Nl80211Attribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU16Value(uint16_t *value) const;
  bool SetU16Value(uint16_t new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

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
  Nl80211U32Attribute(int id, const char *id_string)
      : Nl80211Attribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU32Value(uint32_t *value) const;
  bool SetU32Value(uint32_t new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

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
  Nl80211U64Attribute(int id, const char *id_string)
      : Nl80211Attribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU64Value(uint64_t *value) const;
  bool SetU64Value(uint64_t new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

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
  Nl80211FlagAttribute(int id, const char *id_string)
      : Nl80211Attribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetFlagValue(bool *value) const;
  bool SetFlagValue(bool new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

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
  Nl80211StringAttribute(int id, const char *id_string)
      : Nl80211Attribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetStringValue(std::string *value) const;
  bool SetStringValue(const std::string new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

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

// Nested.

class Nl80211NestedAttribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211NestedAttribute(int id, const char *id_string);
  bool InitFromNlAttr(const nlattr *data) {
    LOG(FATAL) << "Try initializing a _specific_ nested type, instead.";
    return false;
  }
  bool GetNestedValue(base::WeakPtr<AttributeList> *value);
  bool ToString(std::string *value) const {
    return false;  // TODO(wdg):
  }
  virtual ByteString Encode() const {
    return ByteString();  // TODO(wdg): Actually encode the attribute.
  }

 protected:
  AttributeList value_;
};

class Nl80211AttributeCqm : public Nl80211NestedAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeCqm();
  bool InitFromNlAttr(const nlattr *data);
  bool ToString(std::string *value) const {
    return true; // TODO(wdg): Need |ToString|.
  }
};

class Nl80211AttributeStaInfo : public Nl80211NestedAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  explicit Nl80211AttributeStaInfo() :
    Nl80211NestedAttribute(kName, kNameString) {}
  bool InitFromNlAttr(const nlattr *const_data);
  bool AsString(std::string *value) const {
    return true; // TODO(wdg): Need |AsString|.
  }
};

// Raw.

class Nl80211RawAttribute : public Nl80211Attribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  Nl80211RawAttribute(int id, const char *id_string)
      : Nl80211Attribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetRawValue(ByteString *value) const;
  // Not supporting 'set' for raw data.  This type is a "don't know" type to
  // be used for user-bound massages (via InitFromNlAttr).  The 'set' method
  // is intended for building kernel-bound messages and shouldn't be used with
  // raw data.
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const {
    return ByteString();  // TODO(wdg): Actually encode the attribute.
  }
};

class Nl80211AttributeFrame : public Nl80211RawAttribute {
 public:
  static const nl80211_attrs kName;
  static const char kNameString[];
  Nl80211AttributeFrame() : Nl80211RawAttribute(kName, kNameString) {}
};

class Nl80211AttributeGeneric : public Nl80211RawAttribute {
 public:
  explicit Nl80211AttributeGeneric(nl80211_attrs id);
  const char *id_string() const;

 private:
  std::string id_string_;
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


}  // namespace shill

#endif  // SHILL_NLATTRIBUTE_H_
