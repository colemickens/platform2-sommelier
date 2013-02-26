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

NetlinkAttribute::NetlinkAttribute(int id,
                                   const char *id_string,
                                   Type datatype,
                                   const char *datatype_string)
    : has_a_value_(false), id_(id), id_string_(id_string), datatype_(datatype),
      datatype_string_(datatype_string) {}

// static
NetlinkAttribute *NetlinkAttribute::NewNl80211AttributeFromId(int id) {
  scoped_ptr<NetlinkAttribute> attr;
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
      attr.reset(new NetlinkAttributeGeneric(id));
      break;
  }
  return attr.release();
}

// Duplicate attribute data, store in map indexed on |id|.
bool NetlinkAttribute::InitFromNlAttr(const nlattr *other) {
  if (!other) {
    LOG(ERROR) << "NULL data";
    return false;
  }

  data_ = ByteString(reinterpret_cast<const unsigned char *>(other),
                     nla_total_size(nla_len(const_cast<nlattr *>(other))));
  return true;
}

bool NetlinkAttribute::GetU8Value(uint8_t *value) const {
  LOG(ERROR) << "Attribute is not of type 'U8'";
  return false;
}

bool NetlinkAttribute::SetU8Value(uint8_t value) {
  LOG(ERROR) << "Attribute is not of type 'U8'";
  return false;
}

bool NetlinkAttribute::GetU16Value(uint16_t *value) const {
  LOG(ERROR) << "Attribute is not of type 'U16'";
  return false;
}

bool NetlinkAttribute::SetU16Value(uint16_t value) {
  LOG(ERROR) << "Attribute is not of type 'U16'";
  return false;
}

bool NetlinkAttribute::GetU32Value(uint32_t *value) const {
  LOG(ERROR) << "Attribute is not of type 'U32'";
  return false;
}

bool NetlinkAttribute::SetU32Value(uint32_t value) {
  LOG(ERROR) << "Attribute is not of type 'U32'";
  return false;
}

bool NetlinkAttribute::GetU64Value(uint64_t *value) const {
  LOG(ERROR) << "Attribute is not of type 'U64'";
  return false;
}

bool NetlinkAttribute::SetU64Value(uint64_t value) {
  LOG(ERROR) << "Attribute is not of type 'U64'";
  return false;
}

bool NetlinkAttribute::GetFlagValue(bool *value) const {
  LOG(ERROR) << "Attribute is not of type 'Flag'";
  return false;
}

bool NetlinkAttribute::SetFlagValue(bool value) {
  LOG(ERROR) << "Attribute is not of type 'Flag'";
  return false;
}

bool NetlinkAttribute::GetStringValue(string *value) const {
  LOG(ERROR) << "Attribute is not of type 'String'";
  return false;
}

bool NetlinkAttribute::SetStringValue(string value) {
  LOG(ERROR) << "Attribute is not of type 'String'";
  return false;
}

bool NetlinkAttribute::GetNestedValue(WeakPtr<AttributeList> *value) {
  LOG(ERROR) << "Attribute is not of type 'Nested'";
  return false;
}

bool NetlinkAttribute::GetRawValue(ByteString *value) const {
  LOG(ERROR) << "Attribute is not of type 'Raw'";
  return false;
}

void NetlinkAttribute::Print(int log_level, int indent) const {
  string attribute_value;
  SLOG(WiFi, log_level) << HeaderToPrint(indent) << " "
                        << (ToString(&attribute_value) ? attribute_value :
                            "<DOES NOT EXIST>");
}

string NetlinkAttribute::RawToString() const {
  string output = " === RAW: ";

  if (!has_a_value_) {
    StringAppendF(&output, "(empty)");
    return output;
  }

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

string NetlinkAttribute::HeaderToPrint(int indent) const {
  static const int kSpacesPerIndent = 2;
  return StringPrintf("%*s%s(%d) %s %s=",
            indent * kSpacesPerIndent, "",
            id_string(),
            id(),
            datatype_string(),
            ((has_a_value()) ?  "": "UNINITIALIZED "));
}

ByteString NetlinkAttribute::EncodeGeneric(const unsigned char *data,
                                           int bytes) const {
  nlattr header;
  header.nla_type = id();
  header.nla_len = nla_attr_size(bytes);
  ByteString result(reinterpret_cast<unsigned char *>(&header), sizeof(header));
  result.Resize(NLA_HDRLEN);  // Add padding after the header.
  if (data && (bytes != 0)) {
    result.Append(ByteString(data, bytes));
  }
  result.Resize(nla_total_size(bytes));  // Add padding.
  return result;
}

// NetlinkU8Attribute

const char NetlinkU8Attribute::kMyTypeString[] = "uint8_t";
const NetlinkAttribute::Type NetlinkU8Attribute::kType =
    NetlinkAttribute::kTypeU8;

bool NetlinkU8Attribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  uint8_t data = NlaGetU8(input);
  SetU8Value(data);
  return NetlinkAttribute::InitFromNlAttr(input);
}

bool NetlinkU8Attribute::GetU8Value(uint8_t *output) const {
  if (!has_a_value_) {
    SLOG(WiFi, 7) << "U8 attribute " << id_string()
                  << " hasn't been set to any value.";
    return false;
  }
  if (output) {
    *output = value_;
  }
  return true;
}

bool NetlinkU8Attribute::SetU8Value(uint8_t new_value) {
  value_ = new_value;
  has_a_value_ = true;
  return true;
}

bool NetlinkU8Attribute::ToString(string *output) const {
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

ByteString NetlinkU8Attribute::Encode() const {
  return NetlinkAttribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}


// NetlinkU16Attribute

const char NetlinkU16Attribute::kMyTypeString[] = "uint16_t";
const NetlinkAttribute::Type NetlinkU16Attribute::kType =
    NetlinkAttribute::kTypeU16;

bool NetlinkU16Attribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  uint16_t data = NlaGetU16(input);
  SetU16Value(data);
  return NetlinkAttribute::InitFromNlAttr(input);
}

bool NetlinkU16Attribute::GetU16Value(uint16_t *output) const {
  if (!has_a_value_) {
    SLOG(WiFi, 7)  << "U16 attribute " << id_string()
                   << " hasn't been set to any value.";
    return false;
  }
  if (output) {
    *output = value_;
  }
  return true;
}

bool NetlinkU16Attribute::SetU16Value(uint16_t new_value) {
  value_ = new_value;
  has_a_value_ = true;
  return true;
}

bool NetlinkU16Attribute::ToString(string *output) const {
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

ByteString NetlinkU16Attribute::Encode() const {
  return NetlinkAttribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}

// NetlinkU32Attribute::

const char NetlinkU32Attribute::kMyTypeString[] = "uint32_t";
const NetlinkAttribute::Type NetlinkU32Attribute::kType =
    NetlinkAttribute::kTypeU32;

bool NetlinkU32Attribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  uint32_t data = NlaGetU32(input);
  SetU32Value(data);
  return NetlinkAttribute::InitFromNlAttr(input);
}

bool NetlinkU32Attribute::GetU32Value(uint32_t *output) const {
  if (!has_a_value_) {
    SLOG(WiFi, 7)  << "U32 attribute " << id_string()
                   << " hasn't been set to any value.";
    return false;
  }
  if (output) {
    *output = value_;
  }
  return true;
}

bool NetlinkU32Attribute::SetU32Value(uint32_t new_value) {
  value_ = new_value;
  has_a_value_ = true;
  return true;
}

bool NetlinkU32Attribute::ToString(string *output) const {
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

ByteString NetlinkU32Attribute::Encode() const {
  return NetlinkAttribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}

// NetlinkU64Attribute

const char NetlinkU64Attribute::kMyTypeString[] = "uint64_t";
const NetlinkAttribute::Type NetlinkU64Attribute::kType =
    NetlinkAttribute::kTypeU64;

bool NetlinkU64Attribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  uint64_t data = NlaGetU64(input);
  SetU64Value(data);
  return NetlinkAttribute::InitFromNlAttr(input);
}

bool NetlinkU64Attribute::GetU64Value(uint64_t *output) const {
  if (!has_a_value_) {
    SLOG(WiFi, 7)  << "U64 attribute " << id_string()
                   << " hasn't been set to any value.";
    return false;
  }
  if (output) {
    *output = value_;
  }
  return true;
}

bool NetlinkU64Attribute::SetU64Value(uint64_t new_value) {
  value_ = new_value;
  has_a_value_ = true;
  return true;
}

bool NetlinkU64Attribute::ToString(string *output) const {
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

ByteString NetlinkU64Attribute::Encode() const {
  return NetlinkAttribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(&value_), sizeof(value_));
}

// NetlinkFlagAttribute

const char NetlinkFlagAttribute::kMyTypeString[] = "flag";
const NetlinkAttribute::Type NetlinkFlagAttribute::kType =
    NetlinkAttribute::kTypeFlag;

bool NetlinkFlagAttribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  // The existence of the parameter means it's true
  SetFlagValue(true);
  return NetlinkAttribute::InitFromNlAttr(input);
}


bool NetlinkFlagAttribute::GetFlagValue(bool *output) const {
  if (output) {
    // The lack of the existence of the attribute implies 'false'.
    *output = (has_a_value_) ? value_ : false;
  }
  return true;
}

bool NetlinkFlagAttribute::SetFlagValue(bool new_value) {
  value_ = new_value;
  has_a_value_ = true;
  return true;
}

bool NetlinkFlagAttribute::ToString(string *output) const {
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

ByteString NetlinkFlagAttribute::Encode() const {
  if (has_a_value_ && value_) {
    return NetlinkAttribute::EncodeGeneric(NULL, 0);
  }
  return ByteString();  // Encoding of nothing implies 'false'.
}

// NetlinkStringAttribute

const char NetlinkStringAttribute::kMyTypeString[] = "string";
const NetlinkAttribute::Type NetlinkStringAttribute::kType =
    NetlinkAttribute::kTypeString;

bool NetlinkStringAttribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  SetStringValue(NlaGetString(input));
  return NetlinkAttribute::InitFromNlAttr(input);
}

bool NetlinkStringAttribute::GetStringValue(string *output) const {
  if (!has_a_value_) {
    SLOG(WiFi, 7)  << "String attribute " << id_string()
                   << " hasn't been set to any value.";
    return false;
  }
  if (output) {
    *output = value_;
  }
  return true;
}

bool NetlinkStringAttribute::SetStringValue(const string new_value) {
  value_ = new_value;
  has_a_value_ = true;
  return true;
}

bool NetlinkStringAttribute::ToString(string *output) const {
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

ByteString NetlinkStringAttribute::Encode() const {
  return NetlinkAttribute::EncodeGeneric(
      reinterpret_cast<const unsigned char *>(value_.c_str()), value_.size());
}

// NetlinkNestedAttribute
const char NetlinkNestedAttribute::kMyTypeString[] = "nested";
const NetlinkAttribute::Type NetlinkNestedAttribute::kType =
    NetlinkAttribute::kTypeNested;

NetlinkNestedAttribute::NetlinkNestedAttribute(int id,
                                               const char *id_string) :
    NetlinkAttribute(id, id_string, kType, kMyTypeString) {}

bool NetlinkNestedAttribute::GetNestedValue(WeakPtr<AttributeList> *output) {
  if (!has_a_value_) {
    SLOG(WiFi, 7)  << "Nested attribute " << id_string()
                   << " hasn't been set to any value.";
    return false;
  }
  if (output) {
    *output = value_.AsWeakPtr();
  }
  return true;
}

// NetlinkRawAttribute

const char NetlinkRawAttribute::kMyTypeString[] = "<raw>";
const NetlinkAttribute::Type NetlinkRawAttribute::kType =
    NetlinkAttribute::kTypeRaw;

bool NetlinkRawAttribute::InitFromNlAttr(const nlattr *input) {
  if (!input) {
    LOG(ERROR) << "Null |input| parameter";
    return false;
  }

  if (!NetlinkAttribute::InitFromNlAttr(input)) {
    return false;
  }
  has_a_value_ = true;
  return true;
}

bool NetlinkRawAttribute::GetRawValue(ByteString *output) const {
  if (!has_a_value_) {
    SLOG(WiFi, 7)  << "Raw attribute " << id_string()
                   << " hasn't been set to any value.";
    return false;
  }
  if (output) {
    *output = data_;
  }
  return true;
}

bool NetlinkRawAttribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  if (!has_a_value_) {
    SLOG(WiFi, 7)  << "Raw attribute " << id_string()
                   << " hasn't been set to any value.";
    return false;
  }
  const uint8_t *raw_data = reinterpret_cast<const uint8_t *>(data());
  int total_bytes = nla_len(data());
  *output = StringPrintf("%d bytes:", total_bytes);
  for (int i = 0; i < total_bytes; ++i) {
    StringAppendF(output, " 0x%02x", raw_data[i]);
  }
  return true;
}

// Specific Attributes.


const int Nl80211AttributeCookie::kName = NL80211_ATTR_COOKIE;
const char Nl80211AttributeCookie::kNameString[] = "NL80211_ATTR_COOKIE";

const int Nl80211AttributeCqm::kName = NL80211_ATTR_CQM;
const char Nl80211AttributeCqm::kNameString[] = "NL80211_ATTR_CQM";

Nl80211AttributeCqm::Nl80211AttributeCqm()
      : NetlinkNestedAttribute(kName, kNameString) {
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
  has_a_value_ = true;
  return true;
}

const int Nl80211AttributeDisconnectedByAp::kName
    = NL80211_ATTR_DISCONNECTED_BY_AP;
const char Nl80211AttributeDisconnectedByAp::kNameString[]
    = "NL80211_ATTR_DISCONNECTED_BY_AP";

const int Nl80211AttributeDuration::kName = NL80211_ATTR_DURATION;
const char Nl80211AttributeDuration::kNameString[] = "NL80211_ATTR_DURATION";

const int Nl80211AttributeFrame::kName = NL80211_ATTR_FRAME;
const char Nl80211AttributeFrame::kNameString[] = "NL80211_ATTR_FRAME";

const int Nl80211AttributeGeneration::kName = NL80211_ATTR_GENERATION;
const char Nl80211AttributeGeneration::kNameString[]
    = "NL80211_ATTR_GENERATION";

const int Nl80211AttributeIfindex::kName = NL80211_ATTR_IFINDEX;
const char Nl80211AttributeIfindex::kNameString[] = "NL80211_ATTR_IFINDEX";

const int Nl80211AttributeKeyIdx::kName = NL80211_ATTR_KEY_IDX;
const char Nl80211AttributeKeyIdx::kNameString[] = "NL80211_ATTR_KEY_IDX";

const int Nl80211AttributeKeySeq::kName = NL80211_ATTR_KEY_SEQ;
const char Nl80211AttributeKeySeq::kNameString[] = "NL80211_ATTR_KEY_SEQ";

const int Nl80211AttributeKeyType::kName = NL80211_ATTR_KEY_TYPE;
const char Nl80211AttributeKeyType::kNameString[] = "NL80211_ATTR_KEY_TYPE";

const int Nl80211AttributeMac::kName = NL80211_ATTR_MAC;
const char Nl80211AttributeMac::kNameString[] = "NL80211_ATTR_MAC";

const int Nl80211AttributeReasonCode::kName
    = NL80211_ATTR_REASON_CODE;
const char Nl80211AttributeReasonCode::kNameString[]
    = "NL80211_ATTR_REASON_CODE";

const int Nl80211AttributeRegAlpha2::kName = NL80211_ATTR_REG_ALPHA2;
const char Nl80211AttributeRegAlpha2::kNameString[] = "NL80211_ATTR_REG_ALPHA2";

const int Nl80211AttributeRegInitiator::kName
    = NL80211_ATTR_REG_INITIATOR;
const char Nl80211AttributeRegInitiator::kNameString[]
    = "NL80211_ATTR_REG_INITIATOR";

const int Nl80211AttributeRegType::kName = NL80211_ATTR_REG_TYPE;
const char Nl80211AttributeRegType::kNameString[] = "NL80211_ATTR_REG_TYPE";

const int Nl80211AttributeRespIe::kName = NL80211_ATTR_RESP_IE;
const char Nl80211AttributeRespIe::kNameString[] = "NL80211_ATTR_RESP_IE";

const int Nl80211AttributeScanFrequencies::kName
    = NL80211_ATTR_SCAN_FREQUENCIES;
const char Nl80211AttributeScanFrequencies::kNameString[]
    = "NL80211_ATTR_SCAN_FREQUENCIES";

const int Nl80211AttributeScanSsids::kName = NL80211_ATTR_SCAN_SSIDS;
const char Nl80211AttributeScanSsids::kNameString[] = "NL80211_ATTR_SCAN_SSIDS";

const int Nl80211AttributeStaInfo::kName = NL80211_ATTR_STA_INFO;
const char Nl80211AttributeStaInfo::kNameString[] = "NL80211_ATTR_STA_INFO";

bool Nl80211AttributeStaInfo::InitFromNlAttr(const nlattr *const_data) {
  if (!const_data) {
    LOG(ERROR) << "Null |const_data| parameter";
    return false;
  }
  // TODO(wdg): Add code, here.
  has_a_value_ = true;
  return true;
}

const int Nl80211AttributeStatusCode::kName
    = NL80211_ATTR_STATUS_CODE;
const char Nl80211AttributeStatusCode::kNameString[]
    = "NL80211_ATTR_STATUS_CODE";

const int Nl80211AttributeSupportMeshAuth::kName
    = NL80211_ATTR_SUPPORT_MESH_AUTH;
const char Nl80211AttributeSupportMeshAuth::kNameString[]
    = "NL80211_ATTR_SUPPORT_MESH_AUTH";

const int Nl80211AttributeTimedOut::kName = NL80211_ATTR_TIMED_OUT;
const char Nl80211AttributeTimedOut::kNameString[] = "NL80211_ATTR_TIMED_OUT";

const int Nl80211AttributeWiphyFreq::kName = NL80211_ATTR_WIPHY_FREQ;
const char Nl80211AttributeWiphyFreq::kNameString[] = "NL80211_ATTR_WIPHY_FREQ";

const int Nl80211AttributeWiphy::kName = NL80211_ATTR_WIPHY;
const char Nl80211AttributeWiphy::kNameString[] = "NL80211_ATTR_WIPHY";

const int Nl80211AttributeWiphyName::kName = NL80211_ATTR_WIPHY_NAME;
const char Nl80211AttributeWiphyName::kNameString[] = "NL80211_ATTR_WIPHY_NAME";

NetlinkAttributeGeneric::NetlinkAttributeGeneric(int id)
    : NetlinkRawAttribute(id, "unused-string") {
  StringAppendF(&id_string_, "<UNKNOWN ATTRIBUTE %d>", id);
}

const char *NetlinkAttributeGeneric::id_string() const {
  return id_string_.c_str();
}

}  // namespace shill
