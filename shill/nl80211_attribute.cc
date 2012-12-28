// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nl80211_attribute.h"

#include <ctype.h>
#include <linux/nl80211.h>
#include <netlink/attr.h>
#include <netlink/netlink.h>

#include <iomanip>
#include <string>

#include <base/format_macros.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>

#include "shill/attribute_list.h"
#include "shill/logging.h"
#include "shill/scope_logger.h"

using std::string;

using base::StringAppendF;
using base::StringPrintf;
using base::WeakPtr;

namespace shill {

Nl80211Attribute::Nl80211Attribute(int id,
                                   const char *id_string,
                                   Type datatype,
                                   const char *datatype_string)
    : id_(id), id_string_(id_string), datatype_(datatype),
      datatype_string_(datatype_string) {}

// static
Nl80211Attribute *Nl80211Attribute::NewFromName(nl80211_attrs id) {
  scoped_ptr<Nl80211Attribute> attr;
  switch (id) {
    case NL80211_ATTR_COOKIE:
      attr.reset(new Nl80211AttributeCookie());
      break;
    case NL80211_ATTR_CQM:
      attr.reset(new Nl80211AttributeCqm());
      break;
    case NL80211_ATTR_DISCONNECTED_BY_AP:
      attr.reset(new Nl80211AttributeDisconnectedByAp());
      break;
    case NL80211_ATTR_DURATION:
      attr.reset(new Nl80211AttributeDuration());
      break;
    case NL80211_ATTR_FRAME:
      attr.reset(new Nl80211AttributeFrame());
      break;
    case NL80211_ATTR_GENERATION:
      attr.reset(new Nl80211AttributeGeneration());
      break;
    case NL80211_ATTR_IFINDEX:
      attr.reset(new Nl80211AttributeIfindex());
      break;
    case NL80211_ATTR_KEY_IDX:
      attr.reset(new Nl80211AttributeKeyIdx());
      break;
    case NL80211_ATTR_KEY_SEQ:
      attr.reset(new Nl80211AttributeKeySeq());
      break;
    case NL80211_ATTR_KEY_TYPE:
      attr.reset(new Nl80211AttributeKeyType());
      break;
    case NL80211_ATTR_MAC:
      attr.reset(new Nl80211AttributeMac());
      break;
    case NL80211_ATTR_REASON_CODE:
      attr.reset(new Nl80211AttributeReasonCode());
      break;
    case NL80211_ATTR_REG_ALPHA2:
      attr.reset(new Nl80211AttributeRegAlpha2());
      break;
    case NL80211_ATTR_REG_INITIATOR:
      attr.reset(new Nl80211AttributeRegInitiator());
      break;
    case NL80211_ATTR_REG_TYPE:
      attr.reset(new Nl80211AttributeRegType());
      break;
    case NL80211_ATTR_RESP_IE:
      attr.reset(new Nl80211AttributeRespIe());
      break;
    case NL80211_ATTR_SCAN_FREQUENCIES:
      attr.reset(new Nl80211AttributeScanFrequencies());
      break;
    case NL80211_ATTR_SCAN_SSIDS:
      attr.reset(new Nl80211AttributeScanSsids());
      break;
    case NL80211_ATTR_STA_INFO:
      attr.reset(new Nl80211AttributeStaInfo());
      break;
    case NL80211_ATTR_STATUS_CODE:
      attr.reset(new Nl80211AttributeStatusCode());
      break;
    case NL80211_ATTR_SUPPORT_MESH_AUTH:
      attr.reset(new Nl80211AttributeSupportMeshAuth());
      break;
    case NL80211_ATTR_TIMED_OUT:
      attr.reset(new Nl80211AttributeTimedOut());
      break;
    case NL80211_ATTR_WIPHY_FREQ:
      attr.reset(new Nl80211AttributeWiphyFreq());
      break;
    case NL80211_ATTR_WIPHY:
      attr.reset(new Nl80211AttributeWiphy());
      break;
    case NL80211_ATTR_WIPHY_NAME:
      attr.reset(new Nl80211AttributeWiphyName());
      break;
    default:
      attr.reset(new Nl80211AttributeGeneric(id));
      break;
  }
  return attr.release();
}

// Duplicate attribute data, store in map indexed on |id|.
bool Nl80211Attribute::InitFromNlAttr(const nlattr *other) {
  if (!other) {
    LOG(ERROR) << "NULL data";
    return false;
  }

  data_ = ByteString(reinterpret_cast<const unsigned char *>(other),
                     nla_total_size(nla_len(const_cast<nlattr *>(other))));
  return true;
}

bool Nl80211Attribute::GetU8Value(uint8_t *value) const {
  LOG(ERROR) << "Attribute is not of type 'U8'";
  return false;
}

bool Nl80211Attribute::SetU8Value(uint8_t value) {
  LOG(ERROR) << "Attribute is not of type 'U8'";
  return false;
}

bool Nl80211Attribute::GetU16Value(uint16_t *value) const {
  LOG(ERROR) << "Attribute is not of type 'U16'";
  return false;
}

bool Nl80211Attribute::SetU16Value(uint16_t value) {
  LOG(ERROR) << "Attribute is not of type 'U16'";
  return false;
}

bool Nl80211Attribute::GetU32Value(uint32_t *value) const {
  LOG(ERROR) << "Attribute is not of type 'U32'";
  return false;
}

bool Nl80211Attribute::SetU32Value(uint32_t value) {
  LOG(ERROR) << "Attribute is not of type 'U32'";
  return false;
}

bool Nl80211Attribute::GetU64Value(uint64_t *value) const {
  LOG(ERROR) << "Attribute is not of type 'U64'";
  return false;
}

bool Nl80211Attribute::SetU64Value(uint64_t value) {
  LOG(ERROR) << "Attribute is not of type 'U64'";
  return false;
}

bool Nl80211Attribute::GetFlagValue(bool *value) const {
  LOG(ERROR) << "Attribute is not of type 'Flag'";
  return false;
}

bool Nl80211Attribute::SetFlagValue(bool value) {
  LOG(ERROR) << "Attribute is not of type 'Flag'";
  return false;
}

bool Nl80211Attribute::GetStringValue(string *value) const {
  LOG(ERROR) << "Attribute is not of type 'String'";
  return false;
}

bool Nl80211Attribute::SetStringValue(string value) {
  LOG(ERROR) << "Attribute is not of type 'String'";
  return false;
}

bool Nl80211Attribute::GetNestedValue(WeakPtr<AttributeList> *value) {
  LOG(ERROR) << "Attribute is not of type 'Nested'";
  return false;
}

bool Nl80211Attribute::GetRawValue(ByteString *value) const {
  LOG(ERROR) << "Attribute is not of type 'Raw'";
  return false;
}

string Nl80211Attribute::RawToString() const {
  string output = " === RAW: ";

  nlattr *data_nlattr = const_cast<nlattr *>(data());
  uint16_t length = nla_len(data_nlattr);
  StringAppendF(&output, "len=%u", length);

  const uint8_t *const_data = reinterpret_cast<const uint8_t *>(data());

  output.append(" DATA: ");
  for (int i =0 ; i < length; ++i) {
    StringAppendF(&output, "[%d]=%02x ", i, *(const_data)+i);
  }
  output.append(" ==== ");
  return output;
}

ByteString Nl80211Attribute::EncodeGeneric(const unsigned char *data,
                                           int bytes) const {
  nlattr header;
  header.nla_type = id();
  header.nla_len = nla_attr_size(bytes);
  ByteString result(reinterpret_cast<unsigned char *>(&header), sizeof(header));
  result.Append(ByteString(data, bytes));
  result.Resize(nla_total_size(bytes));  // Add padding.
  return result;
}

// Nl80211U8Attribute

const char Nl80211U8Attribute::kMyTypeString[] = "uint8_t";
const Nl80211Attribute::Type Nl80211U8Attribute::kType =
    Nl80211Attribute::kTypeU8;

bool Nl80211U8Attribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  uint8_t data = NlaGetU8(input);
  SetU8Value(data);
  return Nl80211Attribute::InitFromNlAttr(input);
}

bool Nl80211U8Attribute::GetU8Value(uint8_t *output) const {
  if (output) {
    *output = value_;
  }
  return true;
}

bool Nl80211U8Attribute::SetU8Value(uint8_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U8Attribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  uint8_t value;
  if (!GetU8Value(&value))
    return false;
  *output = StringPrintf("%u", value);
  return true;
}

ByteString Nl80211U8Attribute::Encode() const {
  return Nl80211Attribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}


// Nl80211U16Attribute

const char Nl80211U16Attribute::kMyTypeString[] = "uint16_t";
const Nl80211Attribute::Type Nl80211U16Attribute::kType =
    Nl80211Attribute::kTypeU16;

bool Nl80211U16Attribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  uint16_t data = NlaGetU16(input);
  SetU16Value(data);
  return Nl80211Attribute::InitFromNlAttr(input);
}

bool Nl80211U16Attribute::GetU16Value(uint16_t *output) const {
  if (output) {
    *output = value_;
  }
  return true;
}

bool Nl80211U16Attribute::SetU16Value(uint16_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U16Attribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  uint16_t value;
  if (!GetU16Value(&value))
    return false;
  *output = StringPrintf("%u", value);
  return true;
}

ByteString Nl80211U16Attribute::Encode() const {
  return Nl80211Attribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}

// Nl80211U32Attribute::

const char Nl80211U32Attribute::kMyTypeString[] = "uint32_t";
const Nl80211Attribute::Type Nl80211U32Attribute::kType =
    Nl80211Attribute::kTypeU32;

bool Nl80211U32Attribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  uint32_t data = NlaGetU32(input);
  SetU32Value(data);
  return Nl80211Attribute::InitFromNlAttr(input);
}

bool Nl80211U32Attribute::GetU32Value(uint32_t *output) const {
  if (output) {
    *output = value_;
  }
  return true;
}

bool Nl80211U32Attribute::SetU32Value(uint32_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U32Attribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  uint32_t value;
  if (!GetU32Value(&value))
    return false;
  *output = StringPrintf("%" PRIu32, value);
  return true;
}

ByteString Nl80211U32Attribute::Encode() const {
  return Nl80211Attribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}

// Nl80211U64Attribute

const char Nl80211U64Attribute::kMyTypeString[] = "uint64_t";
const Nl80211Attribute::Type Nl80211U64Attribute::kType =
    Nl80211Attribute::kTypeU64;

bool Nl80211U64Attribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  uint64_t data = NlaGetU64(input);
  SetU64Value(data);
  return Nl80211Attribute::InitFromNlAttr(input);
}

bool Nl80211U64Attribute::GetU64Value(uint64_t *output) const {
  if (output) {
    *output = value_;
  }
  return true;
}

bool Nl80211U64Attribute::SetU64Value(uint64_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U64Attribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  uint64_t value;
  if (!GetU64Value(&value))
    return false;
  *output = StringPrintf("%" PRIu64, value);
  return true;
}

ByteString Nl80211U64Attribute::Encode() const {
  return Nl80211Attribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}

// Nl80211FlagAttribute

const char Nl80211FlagAttribute::kMyTypeString[] = "flag";
const Nl80211Attribute::Type Nl80211FlagAttribute::kType =
    Nl80211Attribute::kTypeFlag;

bool Nl80211FlagAttribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  // The existence of the parameter means it's true
  SetFlagValue(true);
  return Nl80211Attribute::InitFromNlAttr(input);
}


bool Nl80211FlagAttribute::GetFlagValue(bool *output) const {
  if (output) {
    *output = value_;
  }
  return true;
}

bool Nl80211FlagAttribute::SetFlagValue(bool new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211FlagAttribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  bool value;
  if (!GetFlagValue(&value))
    return false;
  *output = StringPrintf("%s", value ? "true" : "false");
  return true;
}

ByteString Nl80211FlagAttribute::Encode() const {
  return Nl80211Attribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}

// Nl80211StringAttribute

const char Nl80211StringAttribute::kMyTypeString[] = "string";
const Nl80211Attribute::Type Nl80211StringAttribute::kType =
    Nl80211Attribute::kTypeString;

bool Nl80211StringAttribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  SetStringValue(NlaGetString(input));
  return Nl80211Attribute::InitFromNlAttr(input);
}

bool Nl80211StringAttribute::GetStringValue(string *output) const {
  if (output) {
    *output = value_;
  }
  return true;
}

bool Nl80211StringAttribute::SetStringValue(const string new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211StringAttribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  string value;
  if (!GetStringValue(&value))
    return false;

  *output = StringPrintf("'%s'", value.c_str());
  return true;
}

ByteString Nl80211StringAttribute::Encode() const {
  return Nl80211Attribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(value_.c_str()), value_.size());
}

// Nl80211NestedAttribute
const char Nl80211NestedAttribute::kMyTypeString[] = "nested";
const Nl80211Attribute::Type Nl80211NestedAttribute::kType =
    Nl80211Attribute::kTypeNested;

Nl80211NestedAttribute::Nl80211NestedAttribute(int id,
                                               const char *id_string) :
    Nl80211Attribute(id, id_string, kType, kMyTypeString) {}

bool Nl80211NestedAttribute::GetNestedValue(WeakPtr<AttributeList> *output) {
  if (output) {
    *output = value_.AsWeakPtr();
  }
  return true;
}

// Nl80211RawAttribute

const char Nl80211RawAttribute::kMyTypeString[] = "<raw>";
const Nl80211Attribute::Type Nl80211RawAttribute::kType =
    Nl80211Attribute::kTypeRaw;

bool Nl80211RawAttribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  return Nl80211Attribute::InitFromNlAttr(input);
}

bool Nl80211RawAttribute::GetRawValue(ByteString *output) const {
  if (output) {
    *output = data_;
  }
  return true;
}

bool Nl80211RawAttribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  // TODO(wdg): Make sure that 'data' is valid.
  const uint8_t *raw_data = reinterpret_cast<const uint8_t *>(data());
  int total_bytes = nla_len(data());
  *output = StringPrintf("%d bytes:", total_bytes);
  for (int i = 0; i < total_bytes; ++i) {
    StringAppendF(output, " 0x%02x", raw_data[i]);
  }
  return true;
}

// Specific Attributes.


const nl80211_attrs Nl80211AttributeCookie::kName = NL80211_ATTR_COOKIE;
const char Nl80211AttributeCookie::kNameString[] = "NL80211_ATTR_COOKIE";

const nl80211_attrs Nl80211AttributeCqm::kName = NL80211_ATTR_CQM;
const char Nl80211AttributeCqm::kNameString[] = "NL80211_ATTR_CQM";

Nl80211AttributeCqm::Nl80211AttributeCqm()
      : Nl80211NestedAttribute(kName, kNameString) {
  value_.CreateU32Attribute(NL80211_ATTR_CQM_RSSI_THOLD,
                            "NL80211_ATTR_CQM_RSSI_THOLD");
  value_.CreateU32Attribute(NL80211_ATTR_CQM_RSSI_HYST,
                            "NL80211_ATTR_CQM_RSSI_HYST");
  value_.CreateU32Attribute(NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT,
                            "NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT");
  value_.CreateU32Attribute(NL80211_ATTR_CQM_PKT_LOSS_EVENT,
                            "NL80211_ATTR_CQM_PKT_LOSS_EVENT");
}

bool Nl80211AttributeCqm::InitFromNlAttr(const nlattr *const_data) {
  if (!const_data) {
    LOG(ERROR) << "Null |const_data| parameter";
    return false;
  }
  nlattr *cqm_attr = const_cast<nlattr *>(const_data);

  static const nla_policy kCqmValidationPolicy[NL80211_ATTR_CQM_MAX + 1] = {
    { NLA_U32, 0, 0 },  // Who Knows?
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_THOLD]
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_HYST]
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_PKT_LOSS_EVENT]
  };

  nlattr *cqm[NL80211_ATTR_CQM_MAX + 1];
  if (nla_parse_nested(cqm, NL80211_ATTR_CQM_MAX, cqm_attr,
                       const_cast<nla_policy *>(kCqmValidationPolicy))) {
    LOG(ERROR) << "nla_parse_nested failed";
    return false;
  }

  if (cqm[NL80211_ATTR_CQM_RSSI_THOLD]) {
    value_.SetU32AttributeValue(
        NL80211_ATTR_CQM_RSSI_THOLD,
        nla_get_u32(cqm[NL80211_ATTR_CQM_RSSI_THOLD]));
  }
  if (cqm[NL80211_ATTR_CQM_RSSI_HYST]) {
    value_.SetU32AttributeValue(
        NL80211_ATTR_CQM_RSSI_HYST,
        nla_get_u32(cqm[NL80211_ATTR_CQM_RSSI_HYST]));
  }
  if (cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]) {
    value_.SetU32AttributeValue(
        NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT,
        nla_get_u32(cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]));
  }
  if (cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT]) {
    value_.SetU32AttributeValue(
        NL80211_ATTR_CQM_PKT_LOSS_EVENT,
        nla_get_u32(cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT]));
  }
  return true;
}

const nl80211_attrs Nl80211AttributeDisconnectedByAp::kName
    = NL80211_ATTR_DISCONNECTED_BY_AP;
const char Nl80211AttributeDisconnectedByAp::kNameString[]
    = "NL80211_ATTR_DISCONNECTED_BY_AP";

const nl80211_attrs Nl80211AttributeDuration::kName = NL80211_ATTR_DURATION;
const char Nl80211AttributeDuration::kNameString[] = "NL80211_ATTR_DURATION";

const nl80211_attrs Nl80211AttributeFrame::kName = NL80211_ATTR_FRAME;
const char Nl80211AttributeFrame::kNameString[] = "NL80211_ATTR_FRAME";

const nl80211_attrs Nl80211AttributeGeneration::kName = NL80211_ATTR_GENERATION;
const char Nl80211AttributeGeneration::kNameString[]
    = "NL80211_ATTR_GENERATION";

const nl80211_attrs Nl80211AttributeIfindex::kName = NL80211_ATTR_IFINDEX;
const char Nl80211AttributeIfindex::kNameString[] = "NL80211_ATTR_IFINDEX";

const nl80211_attrs Nl80211AttributeKeyIdx::kName = NL80211_ATTR_KEY_IDX;
const char Nl80211AttributeKeyIdx::kNameString[] = "NL80211_ATTR_KEY_IDX";

const nl80211_attrs Nl80211AttributeKeySeq::kName = NL80211_ATTR_KEY_SEQ;
const char Nl80211AttributeKeySeq::kNameString[] = "NL80211_ATTR_KEY_SEQ";

const nl80211_attrs Nl80211AttributeKeyType::kName = NL80211_ATTR_KEY_TYPE;
const char Nl80211AttributeKeyType::kNameString[] = "NL80211_ATTR_KEY_TYPE";

const nl80211_attrs Nl80211AttributeMac::kName = NL80211_ATTR_MAC;
const char Nl80211AttributeMac::kNameString[] = "NL80211_ATTR_MAC";

const nl80211_attrs Nl80211AttributeReasonCode::kName
    = NL80211_ATTR_REASON_CODE;
const char Nl80211AttributeReasonCode::kNameString[]
    = "NL80211_ATTR_REASON_CODE";

const nl80211_attrs Nl80211AttributeRegAlpha2::kName = NL80211_ATTR_REG_ALPHA2;
const char Nl80211AttributeRegAlpha2::kNameString[] = "NL80211_ATTR_REG_ALPHA2";

const nl80211_attrs Nl80211AttributeRegInitiator::kName
    = NL80211_ATTR_REG_INITIATOR;
const char Nl80211AttributeRegInitiator::kNameString[]
    = "NL80211_ATTR_REG_INITIATOR";

const nl80211_attrs Nl80211AttributeRegType::kName = NL80211_ATTR_REG_TYPE;
const char Nl80211AttributeRegType::kNameString[] = "NL80211_ATTR_REG_TYPE";

const nl80211_attrs Nl80211AttributeRespIe::kName = NL80211_ATTR_RESP_IE;
const char Nl80211AttributeRespIe::kNameString[] = "NL80211_ATTR_RESP_IE";

const nl80211_attrs Nl80211AttributeScanFrequencies::kName
    = NL80211_ATTR_SCAN_FREQUENCIES;
const char Nl80211AttributeScanFrequencies::kNameString[]
    = "NL80211_ATTR_SCAN_FREQUENCIES";

const nl80211_attrs Nl80211AttributeScanSsids::kName = NL80211_ATTR_SCAN_SSIDS;
const char Nl80211AttributeScanSsids::kNameString[] = "NL80211_ATTR_SCAN_SSIDS";

const nl80211_attrs Nl80211AttributeStaInfo::kName = NL80211_ATTR_STA_INFO;
const char Nl80211AttributeStaInfo::kNameString[] = "NL80211_ATTR_STA_INFO";

bool Nl80211AttributeStaInfo::InitFromNlAttr(const nlattr *const_data) {
  if (!const_data) {
    LOG(ERROR) << "Null |const_data| parameter";
    return false;
  }
  // TODO(wdg): Add code, here.
  return true;
}

const nl80211_attrs Nl80211AttributeStatusCode::kName
    = NL80211_ATTR_STATUS_CODE;
const char Nl80211AttributeStatusCode::kNameString[]
    = "NL80211_ATTR_STATUS_CODE";

const nl80211_attrs Nl80211AttributeSupportMeshAuth::kName
    = NL80211_ATTR_SUPPORT_MESH_AUTH;
const char Nl80211AttributeSupportMeshAuth::kNameString[]
    = "NL80211_ATTR_SUPPORT_MESH_AUTH";

const nl80211_attrs Nl80211AttributeTimedOut::kName = NL80211_ATTR_TIMED_OUT;
const char Nl80211AttributeTimedOut::kNameString[] = "NL80211_ATTR_TIMED_OUT";

const nl80211_attrs Nl80211AttributeWiphyFreq::kName = NL80211_ATTR_WIPHY_FREQ;
const char Nl80211AttributeWiphyFreq::kNameString[] = "NL80211_ATTR_WIPHY_FREQ";

const nl80211_attrs Nl80211AttributeWiphy::kName = NL80211_ATTR_WIPHY;
const char Nl80211AttributeWiphy::kNameString[] = "NL80211_ATTR_WIPHY";

const nl80211_attrs Nl80211AttributeWiphyName::kName = NL80211_ATTR_WIPHY_NAME;
const char Nl80211AttributeWiphyName::kNameString[] = "NL80211_ATTR_WIPHY_NAME";

Nl80211AttributeGeneric::Nl80211AttributeGeneric(nl80211_attrs id)
    : Nl80211RawAttribute(id, "unused-string") {
  StringAppendF(&id_string_, "<UNKNOWN ATTRIBUTE %d>", id);
}

const char *Nl80211AttributeGeneric::id_string() const {
  return id_string_.c_str();
}

}  // namespace shill
