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

#include "shill/attribute_list.h"
#include "shill/byte_string.h"
#include "shill/logging.h"
#include "shill/refptr_types.h"

struct nlattr;

namespace shill {

// NetlinkAttribute is an abstract base class that describes an attribute in a
// netlink-80211 message.  Child classes are type-specific and will define
// Get*Value and Set*Value methods (where * is the type).  A second-level of
// child classes exist for each individual attribute type.
//
// An attribute has an id (which is really an enumerated value), a data type,
// and a value.  In an nlattr (the underlying format for an attribute in a
// message), the data is stored as a blob without type information; the writer
// and reader of the attribute must agree on the data type.
class NetlinkAttribute {
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

  NetlinkAttribute(int id, const char *id_string,
                   Type datatype, const char *datatype_string);
  virtual ~NetlinkAttribute() {}

  virtual bool InitFromNlAttr(const nlattr *data);

  // Accessors for the attribute's id and datatype information.
  int id() const { return id_; }
  virtual const char *id_string() const { return id_string_; }
  Type datatype() const { return datatype_; }
  const char *datatype_string() const { return datatype_string_; }

  // TODO(wdg): Since |data| is used, externally, to support |nla_parse_nested|,
  // make it protected once all functionality has been brought inside the
  // NetlinkAttribute classes.
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

  virtual bool GetNestedAttributeList(AttributeListRefPtr *value);
  virtual bool ConstGetNestedAttributeList(
      AttributeListConstRefPtr *value) const;
  virtual bool SetNestedHasAValue();

  virtual bool GetRawValue(ByteString *value) const;

  // Prints the attribute info -- for debugging.
  virtual void Print(int log_level, int indent) const;

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

  bool has_a_value() const { return has_a_value_; }

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

  // Static factories generate the appropriate attribute object from the
  // raw nlattr data.
  static NetlinkAttribute *NewControlAttributeFromId(int id);
  static NetlinkAttribute *NewNl80211AttributeFromId(int id);

 protected:
  // Builds a string to precede a printout of this attribute.
  std::string HeaderToPrint(int indent) const;

  // Encodes the attribute suitably for the attributes in the payload portion
  // of a netlink message suitable for Sockets::Send.  Return value is empty on
  // failure.
  virtual ByteString EncodeGeneric(const unsigned char *data, int bytes) const;

  // Raw data corresponding to the value in any of the child classes.
  // TODO(wdg): When 'data()' is removed, move this to the NetlinkRawAttribute
  // class.
  ByteString data_;

  // True if a value has been assigned to the attribute; false, otherwise.
  bool has_a_value_;

 private:
  int id_;
  const char *id_string_;
  Type datatype_;
  const char *datatype_string_;
};

// U8.

class NetlinkU8Attribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkU8Attribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU8Value(uint8_t *value) const;
  bool SetU8Value(uint8_t new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  uint8_t value_;
};

class Nl80211AttributeKeyIdx : public NetlinkU8Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeyIdx() : NetlinkU8Attribute(kName, kNameString) {}
};

class Nl80211AttributeRegType : public NetlinkU8Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegType() : NetlinkU8Attribute(kName, kNameString) {}
};

// U16.

class NetlinkU16Attribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkU16Attribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU16Value(uint16_t *value) const;
  bool SetU16Value(uint16_t new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  uint16_t value_;
};

class Nl80211AttributeReasonCode : public NetlinkU16Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeReasonCode() : NetlinkU16Attribute(kName, kNameString) {}
};

class Nl80211AttributeStatusCode : public NetlinkU16Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeStatusCode() : NetlinkU16Attribute(kName, kNameString) {}
};

// U32.

class NetlinkU32Attribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkU32Attribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU32Value(uint32_t *value) const;
  bool SetU32Value(uint32_t new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  uint32_t value_;
};

class Nl80211AttributeDuration : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeDuration() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeGeneration : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeGeneration() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeIfindex : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeIfindex() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeKeyType : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeyType() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeRegInitiator : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegInitiator() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeWiphy : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphy() : NetlinkU32Attribute(kName, kNameString) {}
};

class Nl80211AttributeWiphyFreq : public NetlinkU32Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphyFreq() : NetlinkU32Attribute(kName, kNameString) {}
};

// U64.

class NetlinkU64Attribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkU64Attribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetU64Value(uint64_t *value) const;
  bool SetU64Value(uint64_t new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  uint64_t value_;
};

class Nl80211AttributeCookie : public NetlinkU64Attribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeCookie() : NetlinkU64Attribute(kName, kNameString) {}
};

// Flag.

class NetlinkFlagAttribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkFlagAttribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetFlagValue(bool *value) const;
  bool SetFlagValue(bool new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  bool value_;
};

class Nl80211AttributeDisconnectedByAp : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeDisconnectedByAp() :
    NetlinkFlagAttribute(kName, kNameString) {}
};

class Nl80211AttributeSupportMeshAuth : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeSupportMeshAuth() :
    NetlinkFlagAttribute(kName, kNameString) {}
};

class Nl80211AttributeTimedOut : public NetlinkFlagAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeTimedOut() : NetlinkFlagAttribute(kName, kNameString) {}
};

// String.

class NetlinkStringAttribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkStringAttribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  bool InitFromNlAttr(const nlattr *data);
  bool GetStringValue(std::string *value) const;
  bool SetStringValue(const std::string new_value);
  bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  std::string value_;
};

class Nl80211AttributeRegAlpha2 : public NetlinkStringAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRegAlpha2() : NetlinkStringAttribute(kName, kNameString) {}
};

class Nl80211AttributeWiphyName : public NetlinkStringAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeWiphyName() : NetlinkStringAttribute(kName, kNameString) {}
};

// Nested.

class NetlinkNestedAttribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkNestedAttribute(int id, const char *id_string);
  bool InitFromNlAttr(const nlattr *data) {
    LOG(FATAL) << "Try initializing a _specific_ nested type, instead.";
    return false;
  }
  virtual bool GetNestedAttributeList(AttributeListRefPtr *value);
  virtual bool ConstGetNestedAttributeList(
      AttributeListConstRefPtr *value) const;
  virtual bool SetNestedHasAValue();
  virtual bool ToString(std::string *value) const {
    return false;  // TODO(wdg): Actually generate a string, here.
  }
  virtual ByteString Encode() const {
    return ByteString();  // TODO(wdg): Actually encode the attribute.
  }

 protected:
  // Describes a single nested attribute.  Provides the expected values and
  // type (including further nesting).  Normally, an array of these, one for
  // each attribute at one level of nesting is presented, along with the data
  // to be parsed, to |InitNestedFromNlAttr|.  If the attributes on one level
  // represent an array, a single |NestedData| is provided and |is_array| is
  // set (note that one level of nesting either contains _only_ an array or
  // _no_ array).
  struct NestedData {
    nla_policy policy;
    const char *attribute_name;
    const NestedData *deeper_nesting;
    size_t deeper_nesting_size;
    bool is_array;
    // TODO(wdg): Add function pointer for a custom attribute parser.
  };

  // Builds an AttributeList (|list|) that contains all of the attriubtes in
  // |const_data|.  |const_data| should point to the enclosing nested attribute
  // header; for the example of the nested attribute NL80211_ATTR_CQM:
  //    nlattr::nla_type: NL80211_ATTR_CQM <-- const_data points here
  //    nlattr::nla_len: 12 bytes
  //      nlattr::nla_type: PKT_LOSS_EVENT (1st and only nested attribute)
  //      nlattr::nla_len: 8 bytes
  //      <data>: 0x32
  // One can assemble (hence, disassemble) a set of child attributes under a
  // nested attribute parent as an array of elements or as a structure.
  //
  // The data is parsed using the expected configuration in |nested_template|.
  // If the code expects an array, it will pass a single template element and
  // mark that as an array.
  static bool InitNestedFromNlAttr(AttributeList *list,
                                   const NestedData *nested_template,
                                   size_t nested_template_size,
                                   const nlattr *const_data);

  static bool ParseNestedArray(AttributeList *list,
                               const NestedData &nested_template,
                               const nlattr *const_data);

  static bool ParseNestedStructure(AttributeList *list,
                                   const NestedData *nested_template,
                                   size_t nested_template_size,
                                   const nlattr *const_data);

  // Helper function used by InitNestedFromNlAttr to add a single child
  // attribute to a nested attribute.
  static void AddAttributeToNested(AttributeList *list, uint16_t type, size_t i,
                                   const std::string &attribute_name,
                                   const nlattr &attr,
                                   const NestedData &nested_data);
  AttributeListRefPtr value_;
};

class Nl80211AttributeCqm : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeCqm();
  bool InitFromNlAttr(const nlattr *data);
  bool ToString(std::string *value) const {
    return false;  // TODO(wdg): Need |ToString|.
  }
};

class Nl80211AttributeStaInfo : public NetlinkNestedAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  explicit Nl80211AttributeStaInfo() :
    NetlinkNestedAttribute(kName, kNameString) {}
  bool InitFromNlAttr(const nlattr *const_data);
  bool ToString(std::string *value) const {
    return false;  // TODO(wdg): Need |ToString|.
  }
};

// Raw.

class NetlinkRawAttribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkRawAttribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
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

class Nl80211AttributeFrame : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeFrame() : NetlinkRawAttribute(kName, kNameString) {}
};

class NetlinkAttributeGeneric : public NetlinkRawAttribute {
 public:
  explicit NetlinkAttributeGeneric(int id);
  const char *id_string() const;

 private:
  std::string id_string_;
};

class Nl80211AttributeKeySeq : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeKeySeq() : NetlinkRawAttribute(kName, kNameString) {}
};

class Nl80211AttributeMac : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeMac() : NetlinkRawAttribute(kName, kNameString) {}
};

class Nl80211AttributeRespIe : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeRespIe() : NetlinkRawAttribute(kName, kNameString) {}
};

class Nl80211AttributeScanFrequencies : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeScanFrequencies() : NetlinkRawAttribute(kName, kNameString) {}
};

class Nl80211AttributeScanSsids : public NetlinkRawAttribute {
 public:
  static const int kName;
  static const char kNameString[];
  Nl80211AttributeScanSsids() : NetlinkRawAttribute(kName, kNameString) {}
};


}  // namespace shill

#endif  // SHILL_NLATTRIBUTE_H_
