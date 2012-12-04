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

#include "shill/logging.h"
#include "shill/scope_logger.h"

using std::string;

using base::StringAppendF;
using base::StringPrintf;

namespace shill {

Nl80211Attribute::Nl80211Attribute(enum nl80211_attrs name,
                                   const char *name_string,
                                   Type type,
                                   const char *type_string)
    : name_(name), name_string_(name_string), type_(type),
      type_string_(type_string) {}

// static
Nl80211Attribute *Nl80211Attribute::NewFromNlAttr(nl80211_attrs name,
                                                  const nlattr *data) {
  scoped_ptr<Nl80211Attribute> attr;
  switch (name) {
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
      attr.reset(new Nl80211AttributeGeneric(name));
      break;
  }
  attr->InitFromNlAttr(data);
  return attr.release();
}

// Duplicate attribute data, store in map indexed on |name|.
bool Nl80211Attribute::InitFromNlAttr(const nlattr *other) {
  if (!other) {
    LOG(ERROR) << "NULL data";
    return false;
  }

  data_ = ByteString(reinterpret_cast<const unsigned char *>(other),
                     nla_total_size(nla_len(const_cast<nlattr *>(other))));
  return true;
}

// Copies raw attribute data but not the header.
bool Nl80211Attribute::GetRawData(ByteString *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }

  *output = data_;
  return true;
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
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  *output = value_;
  return true;
}

bool Nl80211U8Attribute::SetU8Value(uint8_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U8Attribute::AsString(string *output) const {
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
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  *output = value_;
  return true;
}

bool Nl80211U16Attribute::SetU16Value(uint16_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U16Attribute::AsString(string *output) const {
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
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  *output = value_;
  return true;
}

bool Nl80211U32Attribute::SetU32Value(uint32_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U32Attribute::AsString(string *output) const {
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
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  *output = value_;
  return true;
}

bool Nl80211U64Attribute::SetU64Value(uint64_t new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211U64Attribute::AsString(string *output) const {
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
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  *output = value_;
  return true;
}

bool Nl80211FlagAttribute::SetFlagValue(bool new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211FlagAttribute::AsString(string *output) const {
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
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  *output = value_;
  return true;
}

bool Nl80211StringAttribute::SetStringValue(const string new_value) {
  value_ = new_value;
  return true;
}

bool Nl80211StringAttribute::AsString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  return GetStringValue(output);
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
  if (!output) {
    LOG(ERROR) << "NULL |output|";
    return false;
  }
  *output = data_;
  return true;
}

bool Nl80211RawAttribute::AsString(string *output) const {
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

Nl80211AttributeGeneric::Nl80211AttributeGeneric(nl80211_attrs name)
    : Nl80211RawAttribute(name, "unused-string") {
  StringAppendF(&name_string_, "<UNKNOWN ATTRIBUTE %d>", name);
}

const char *Nl80211AttributeGeneric::name_string() const {
  return name_string_.c_str();
}

}  // namespace shill
