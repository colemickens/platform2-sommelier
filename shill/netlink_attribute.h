// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NETLINK_ATTRIBUTE_H_
#define SHILL_NETLINK_ATTRIBUTE_H_

#include <netlink/attr.h>

#include <string>

#include <base/basictypes.h>

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

  // Static factories generate the appropriate attribute object from the
  // raw nlattr data.
  static NetlinkAttribute *NewControlAttributeFromId(int id);
  static NetlinkAttribute *NewNl80211AttributeFromId(int id);

  virtual bool InitFromNlAttr(const nlattr *data);

  // Accessors for the attribute's id and datatype information.
  int id() const { return id_; }
  virtual const char *id_string() const { return id_string_.c_str(); }
  Type datatype() const { return datatype_; }
  const char *datatype_string() const { return datatype_string_; }

  // Accessors.  Return false if request is made on wrong type of attribute.
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
  virtual bool SetRawValue(const ByteString value);

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

  // Wrappers for libnl parsers.  Needed because, while |nla_get_*| don't
  // change their arguments, these methods don't declare themselves as 'const'.
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
  // Builds a string to precede a printout of this attribute.
  std::string HeaderToPrint(int indent) const;

  // Encodes the attribute suitably for the attributes in the payload portion
  // of a netlink message suitable for Sockets::Send.  Return value is empty on
  // failure.
  ByteString EncodeGeneric(const unsigned char *data, size_t num_bytes) const;

  // Attribute data (NOT including the nlattr header) corresponding to the
  // value in any of the child classes.
  ByteString data_;

  // True if a value has been assigned to the attribute; false, otherwise.
  bool has_a_value_;

 private:
  int id_;
  std::string id_string_;
  Type datatype_;
  const char *datatype_string_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkAttribute);
};

class NetlinkU8Attribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkU8Attribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  virtual bool InitFromNlAttr(const nlattr *data);
  virtual bool GetU8Value(uint8_t *value) const;
  virtual bool SetU8Value(uint8_t new_value);
  virtual bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  uint8_t value_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkU8Attribute);
};

class NetlinkU16Attribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkU16Attribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  virtual bool InitFromNlAttr(const nlattr *data);
  virtual bool GetU16Value(uint16_t *value) const;
  virtual bool SetU16Value(uint16_t new_value);
  virtual bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  uint16_t value_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkU16Attribute);
};

class NetlinkU32Attribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkU32Attribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  virtual bool InitFromNlAttr(const nlattr *data);
  virtual bool GetU32Value(uint32_t *value) const;
  virtual bool SetU32Value(uint32_t new_value);
  virtual bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  uint32_t value_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkU32Attribute);
};

class NetlinkU64Attribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkU64Attribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  virtual bool InitFromNlAttr(const nlattr *data);
  virtual bool GetU64Value(uint64_t *value) const;
  virtual bool SetU64Value(uint64_t new_value);
  virtual bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  uint64_t value_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkU64Attribute);
};

class NetlinkFlagAttribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkFlagAttribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  virtual bool InitFromNlAttr(const nlattr *data);
  virtual bool GetFlagValue(bool *value) const;
  virtual bool SetFlagValue(bool new_value);
  virtual bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  bool value_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkFlagAttribute);
};

class NetlinkStringAttribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkStringAttribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  virtual bool InitFromNlAttr(const nlattr *data);
  virtual bool GetStringValue(std::string *value) const;
  virtual bool SetStringValue(const std::string new_value);
  virtual bool ToString(std::string *value) const;
  virtual ByteString Encode() const;
  std::string value() const { return value_; }
  void set_value(const std::string &value) { value_ = value; }

 private:
  std::string value_;
  DISALLOW_COPY_AND_ASSIGN(NetlinkStringAttribute);
};

// SSID attributes are just string attributes with different output semantics.
class NetlinkSsidAttribute : public NetlinkStringAttribute {
 public:
  NetlinkSsidAttribute(int id, const char *id_string)
      : NetlinkStringAttribute(id, id_string) {}

  // NOTE: |ToString| or |Print| must be used for logging to allow scrubbing.
  virtual bool ToString(std::string *output) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetlinkSsidAttribute);
};

class NetlinkNestedAttribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkNestedAttribute(int id, const char *id_string);
  virtual bool InitFromNlAttr(const nlattr *data);
  virtual bool GetNestedAttributeList(AttributeListRefPtr *value);
  virtual bool ConstGetNestedAttributeList(
      AttributeListConstRefPtr *value) const;
  virtual bool SetNestedHasAValue();
  virtual void Print(int log_level, int indent) const;
  virtual bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 protected:
  // Describes a single nested attribute.  Provides the expected values and
  // type (including further nesting).  Normally, an array of these, one for
  // each attribute at one level of nesting is presented, along with the data
  // to be parsed, to |InitNestedFromNlAttr|.  If the attributes on one level
  // represent an array, a single |NestedData| is provided and |is_array| is
  // set (note that one level of nesting either contains _only_ an array or
  // _no_ array).
  struct NestedData {
    typedef base::Callback<bool (AttributeList *list, size_t id,
                                 const std::string &attribute_name,
                                 ByteString data)> AttributeParser;
    typedef std::vector<NestedData> NestedDataVector;

    NestedData();
    NestedData(uint16_t type, std::string attribute_name, bool is_array);
    NestedData(uint16_t type, std::string attribute_name, bool is_array,
               const AttributeParser &parse_attribute);
    uint16_t type;
    std::string attribute_name;
    NestedDataVector deeper_nesting;
    bool is_array;
    // Closure that overrides the usual parsing of this attribute.  A non-NULL
    // value for |parse_attribute| will cause the software to ignore the other
    // members of the |NestedData| structure.
    AttributeParser parse_attribute;
  };

  NestedData::NestedDataVector nested_template_;

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
  static bool InitNestedFromNlAttr(
      AttributeList *list, const NestedData::NestedDataVector &templates,
      const nlattr *const_data);

  static bool ParseNestedArray(
      AttributeList *list, const NestedData &templates,
      const nlattr *const_data);

  static bool ParseNestedStructure(
      AttributeList *list, const NestedData::NestedDataVector &templates,
      const nlattr *const_data);

  // Helper function used by InitNestedFromNlAttr to add a single child
  // attribute to a nested attribute.
  static void AddAttributeToNested(AttributeList *list, uint16_t type, size_t i,
                                   const std::string &attribute_name,
                                   const nlattr &attr,
                                   const NestedData &nested_data);
  AttributeListRefPtr value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetlinkNestedAttribute);
};

class NetlinkRawAttribute : public NetlinkAttribute {
 public:
  static const char kMyTypeString[];
  static const Type kType;
  NetlinkRawAttribute(int id, const char *id_string)
      : NetlinkAttribute(id, id_string, kType, kMyTypeString) {}
  virtual bool InitFromNlAttr(const nlattr *data);
  // Gets the value of the data (the header is not stored).
  virtual bool GetRawValue(ByteString *value) const;
  // Should set the value of the data (not the attribute header).
  virtual bool SetRawValue(const ByteString value);
  virtual bool ToString(std::string *value) const;
  virtual ByteString Encode() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetlinkRawAttribute);
};

class NetlinkAttributeGeneric : public NetlinkRawAttribute {
 public:
  explicit NetlinkAttributeGeneric(int id);
  virtual const char *id_string() const;

 private:
  std::string id_string_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkAttributeGeneric);
};

}  // namespace shill

#endif  // SHILL_NETLINK_ATTRIBUTE_H_
