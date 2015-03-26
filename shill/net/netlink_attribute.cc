// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/netlink_attribute.h"

#include <netlink/attr.h>

#include <cctype>
#include <map>
#include <memory>
#include <string>

#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "shill/net/attribute_list.h"
#include "shill/net/control_netlink_attribute.h"
#include "shill/net/netlink_message.h"
#include "shill/net/nl80211_attribute.h"

using std::map;
using std::string;
using std::unique_ptr;

using base::StringAppendF;
using base::StringPrintf;

namespace shill {

// This is a type-corrected copy of |nla_for_each_nested| from libnl.
#define nla_for_each_nested_type_corrected(pos, nla, rem) \
  for (pos = reinterpret_cast<struct nlattr *>(nla_data(nla)), \
       rem = nla_len(nla); \
    nla_ok(pos, rem); \
    pos = nla_next(pos, &(rem)))

NetlinkAttribute::NetlinkAttribute(int id,
                                   const char *id_string,
                                   Type datatype,
                                   const char *datatype_string)
    : has_a_value_(false), id_(id), id_string_(id_string), datatype_(datatype),
      datatype_string_(datatype_string) {}

// static
NetlinkAttribute *NetlinkAttribute::NewNl80211AttributeFromId(
    NetlinkMessage::MessageContext context, int id) {
  unique_ptr<NetlinkAttribute> attr;
  switch (id) {
    case NL80211_ATTR_BSS:
      attr.reset(new Nl80211AttributeBss());
      break;
    case NL80211_ATTR_CIPHER_SUITES:
      attr.reset(new Nl80211AttributeCipherSuites());
      break;
    case NL80211_ATTR_CONTROL_PORT_ETHERTYPE:
      attr.reset(new Nl80211AttributeControlPortEthertype());
      break;
    case NL80211_ATTR_COOKIE:
      attr.reset(new Nl80211AttributeCookie());
      break;
    case NL80211_ATTR_CQM:
      attr.reset(new Nl80211AttributeCqm());
      break;
    case NL80211_ATTR_DEVICE_AP_SME:
      attr.reset(new Nl80211AttributeDeviceApSme());
      break;
    case NL80211_ATTR_DISCONNECTED_BY_AP:
      attr.reset(new Nl80211AttributeDisconnectedByAp());
      break;
    case NL80211_ATTR_DURATION:
      attr.reset(new Nl80211AttributeDuration());
      break;
    case NL80211_ATTR_FEATURE_FLAGS:
      attr.reset(new Nl80211AttributeFeatureFlags());
      break;
    case NL80211_ATTR_FRAME:
      attr.reset(new Nl80211AttributeFrame());
      break;
    case NL80211_ATTR_GENERATION:
      attr.reset(new Nl80211AttributeGeneration());
      break;
    case NL80211_ATTR_HT_CAPABILITY_MASK:
      attr.reset(new Nl80211AttributeHtCapabilityMask());
      break;
    case NL80211_ATTR_IFINDEX:
      attr.reset(new Nl80211AttributeIfindex());
      break;
    case NL80211_ATTR_IFTYPE:
      attr.reset(new Nl80211AttributeIftype());
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
    case NL80211_ATTR_MAX_MATCH_SETS:
      attr.reset(new Nl80211AttributeMaxMatchSets());
      break;
    case NL80211_ATTR_MAX_NUM_PMKIDS:
      attr.reset(new Nl80211AttributeMaxNumPmkids());
      break;
    case NL80211_ATTR_MAX_NUM_SCAN_SSIDS:
      attr.reset(new Nl80211AttributeMaxNumScanSsids());
      break;
    case NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS:
      attr.reset(new Nl80211AttributeMaxNumSchedScanSsids());
      break;
    case NL80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION:
      attr.reset(new Nl80211AttributeMaxRemainOnChannelDuration());
      break;
    case NL80211_ATTR_MAX_SCAN_IE_LEN:
      attr.reset(new Nl80211AttributeMaxScanIeLen());
      break;
    case NL80211_ATTR_MAX_SCHED_SCAN_IE_LEN:
      attr.reset(new Nl80211AttributeMaxSchedScanIeLen());
      break;
    case NL80211_ATTR_OFFCHANNEL_TX_OK:
      attr.reset(new Nl80211AttributeOffchannelTxOk());
      break;
    case NL80211_ATTR_PROBE_RESP_OFFLOAD:
      attr.reset(new Nl80211AttributeProbeRespOffload());
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
    case NL80211_ATTR_ROAM_SUPPORT:
      attr.reset(new Nl80211AttributeRoamSupport());
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
    case NL80211_ATTR_SUPPORT_AP_UAPSD:
      attr.reset(new Nl80211AttributeSupportApUapsd());
      break;
    case NL80211_ATTR_SUPPORT_IBSS_RSN:
      attr.reset(new Nl80211AttributeSupportIbssRsn());
      break;
    case NL80211_ATTR_SUPPORT_MESH_AUTH:
      attr.reset(new Nl80211AttributeSupportMeshAuth());
      break;
    case NL80211_ATTR_SUPPORTED_IFTYPES:
      attr.reset(new Nl80211AttributeSupportedIftypes());
      break;
    case NL80211_ATTR_TDLS_EXTERNAL_SETUP:
      attr.reset(new Nl80211AttributeTdlsExternalSetup());
      break;
    case NL80211_ATTR_TDLS_SUPPORT:
      attr.reset(new Nl80211AttributeTdlsSupport());
      break;
    case NL80211_ATTR_TIMED_OUT:
      attr.reset(new Nl80211AttributeTimedOut());
      break;
    case NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX:
      attr.reset(new Nl80211AttributeWiphyAntennaAvailRx());
      break;
    case NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX:
      attr.reset(new Nl80211AttributeWiphyAntennaAvailTx());
      break;
    case NL80211_ATTR_WIPHY_ANTENNA_RX:
      attr.reset(new Nl80211AttributeWiphyAntennaRx());
      break;
    case NL80211_ATTR_WIPHY_ANTENNA_TX:
      attr.reset(new Nl80211AttributeWiphyAntennaTx());
      break;
    case NL80211_ATTR_WIPHY_BANDS:
      attr.reset(new Nl80211AttributeWiphyBands());
      break;
    case NL80211_ATTR_WIPHY_COVERAGE_CLASS:
      attr.reset(new Nl80211AttributeWiphyCoverageClass());
      break;
    case NL80211_ATTR_WIPHY_FRAG_THRESHOLD:
      attr.reset(new Nl80211AttributeWiphyFragThreshold());
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
    case NL80211_ATTR_WIPHY_RETRY_LONG:
      attr.reset(new Nl80211AttributeWiphyRetryLong());
      break;
    case NL80211_ATTR_WIPHY_RETRY_SHORT:
      attr.reset(new Nl80211AttributeWiphyRetryShort());
      break;
    case NL80211_ATTR_WIPHY_RTS_THRESHOLD:
      attr.reset(new Nl80211AttributeWiphyRtsThreshold());
      break;
    case NL80211_ATTR_WOWLAN_TRIGGERS:
      attr.reset(new Nl80211AttributeWowlanTriggers(context));
      break;
    case NL80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED:
      attr.reset(new Nl80211AttributeWowlanTriggersSupported());
      break;
    case NL80211_ATTR_SURVEY_INFO:
      attr.reset(new Nl80211AttributeSurveyInfo());
      break;
    default:
      attr.reset(new NetlinkAttributeGeneric(id));
      break;
  }
  return attr.release();
}

// static
NetlinkAttribute *NetlinkAttribute::NewControlAttributeFromId(int id) {
  unique_ptr<NetlinkAttribute> attr;
  switch (id) {
    case CTRL_ATTR_FAMILY_ID:
      attr.reset(new ControlAttributeFamilyId());
      break;
    case CTRL_ATTR_FAMILY_NAME:
      attr.reset(new ControlAttributeFamilyName());
      break;
    case CTRL_ATTR_VERSION:
      attr.reset(new ControlAttributeVersion());
      break;
    case CTRL_ATTR_HDRSIZE:
      attr.reset(new ControlAttributeHdrSize());
      break;
    case CTRL_ATTR_MAXATTR:
      attr.reset(new ControlAttributeMaxAttr());
      break;
    case CTRL_ATTR_OPS:
      attr.reset(new ControlAttributeAttrOps());
      break;
    case CTRL_ATTR_MCAST_GROUPS:
      attr.reset(new ControlAttributeMcastGroups());
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

  data_ = ByteString(
      reinterpret_cast<char *>(nla_data(const_cast<nlattr *>(other))),
      nla_len(const_cast<nlattr *>(other)));
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

bool NetlinkAttribute::GetNestedAttributeList(AttributeListRefPtr *value) {
  LOG(ERROR) << "Attribute is not of type 'Nested'";
  return false;
}

bool NetlinkAttribute::ConstGetNestedAttributeList(
    AttributeListConstRefPtr *value) const {
  LOG(ERROR) << "Attribute is not of type 'Nested'";
  return false;
}

bool NetlinkAttribute::SetNestedHasAValue() {
  LOG(ERROR) << "Attribute is not of type 'Nested'";
  return false;
}

bool NetlinkAttribute::GetRawValue(ByteString *value) const {
  LOG(ERROR) << "Attribute is not of type 'Raw'";
  return false;
}

bool NetlinkAttribute::SetRawValue(const ByteString new_value) {
  LOG(ERROR) << "Attribute is not of type 'Raw'";
  return false;
}

void NetlinkAttribute::Print(int log_level, int indent) const {
  string attribute_value;
  VLOG(log_level) << HeaderToPrint(indent) << " "
                  << (ToString(&attribute_value) ? attribute_value :
                      "<DOES NOT EXIST>");
}

string NetlinkAttribute::RawToString() const {
  string output = " === RAW: ";

  if (!has_a_value_) {
    StringAppendF(&output, "(empty)");
    return output;
  }

  uint16_t length = data_.GetLength();
  const uint8_t *const_data = data_.GetConstData();

  StringAppendF(&output, "len=%u", length);
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
                                           size_t num_bytes) const {
  ByteString result;
  if (has_a_value_) {
    nlattr header;
    header.nla_type = id();
    header.nla_len = nla_attr_size(num_bytes);
    result = ByteString(reinterpret_cast<unsigned char *>(&header),
                        sizeof(header));
    result.Resize(NLA_HDRLEN);  // Add padding after the header.
    if (data && (num_bytes != 0)) {
      result.Append(ByteString(data, num_bytes));
    }
    result.Resize(nla_total_size(num_bytes));  // Add padding.
  }
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
    VLOG(7) << "U8 attribute " << id_string()
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
    VLOG(7)  << "U16 attribute " << id_string()
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
    VLOG(7)  << "U32 attribute " << id_string()
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
    VLOG(7)  << "U64 attribute " << id_string()
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
    return NetlinkAttribute::EncodeGeneric(nullptr, 0);
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
    VLOG(7)  << "String attribute " << id_string()
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
      reinterpret_cast<const unsigned char *>(value_.c_str()),
      value_.size() + 1);
}

// SSID attribute.

bool NetlinkSsidAttribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  string value;
  if (!GetStringValue(&value))
    return false;

  string temp;
  for (const auto &chr : value) {
    // Replace '[' and ']' (in addition to non-printable characters) so that
    // it's easy to match the right substring through a non-greedy regex.
    if (chr == '[' || chr == ']' || !std::isprint(chr)) {
      base::StringAppendF(&temp, "\\x%02x", chr);
    } else {
      temp += chr;
    }
  }
  *output = StringPrintf("[SSID=%s]", temp.c_str());

  return true;
}

// NetlinkNestedAttribute

const char NetlinkNestedAttribute::kMyTypeString[] = "nested";
const NetlinkAttribute::Type NetlinkNestedAttribute::kType =
    NetlinkAttribute::kTypeNested;
const size_t NetlinkNestedAttribute::kArrayAttrEnumVal = 0;

NetlinkNestedAttribute::NetlinkNestedAttribute(int id,
                                               const char *id_string) :
    NetlinkAttribute(id, id_string, kType, kMyTypeString),
    value_(new AttributeList) {}

ByteString NetlinkNestedAttribute::Encode() const {
  // Encode attribute header.
  nlattr header;
  header.nla_type = id();
  header.nla_len = nla_attr_size(sizeof(header));
  ByteString result(reinterpret_cast<unsigned char *>(&header), sizeof(header));
  result.Resize(NLA_HDRLEN);  // Add padding after the header.

  // Encode all nested attributes.
  map<int, AttributeList::AttributePointer>::const_iterator attribute;
  for (attribute = value_->attributes_.begin();
       attribute != value_->attributes_.end();
       ++attribute) {
    // Each attribute appends appropriate padding so it's not necessary to
    // re-add padding.
    result.Append(attribute->second->Encode());
  }

  // Go back and fill-in the size.
  nlattr *new_header = reinterpret_cast<nlattr *>(result.GetData());
  new_header->nla_len = result.GetLength();

  return result;
}

void NetlinkNestedAttribute::Print(int log_level, int indent) const {
  VLOG(log_level) << HeaderToPrint(indent);
  value_->Print(log_level, indent + 1);
}

bool NetlinkNestedAttribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }

  // This should never be called (attribute->ToString is only called
  // from attribute->Print but NetlinkNestedAttribute::Print doesn't call
  // |ToString|.  Still, we should print something in case we got here
  // accidentally.
  LOG(WARNING) << "It is unexpected for this method to be called.";
  output->append("<Nested Attribute>");
  return true;
}

bool NetlinkNestedAttribute::InitFromNlAttr(const nlattr *const_data) {
  if (!InitNestedFromNlAttr(value_.get(), nested_template_, const_data)) {
    LOG(ERROR) << "InitNestedFromNlAttr() failed";
    return false;
  }
  has_a_value_ = true;
  return true;
}

bool NetlinkNestedAttribute::GetNestedAttributeList(
    AttributeListRefPtr *output) {
  // Not checking |has_a_value| since GetNestedAttributeList is called to get
  // a newly created AttributeList in order to have something to which to add
  // attributes.
  if (output) {
    *output = value_;
  }
  return true;
}

bool NetlinkNestedAttribute::ConstGetNestedAttributeList(
    AttributeListConstRefPtr *output) const {
  if (!has_a_value_) {
    LOG(ERROR) << "Attribute does not exist.";
    return false;
  }
  if (output) {
    *output = value_;
  }
  return true;
}

bool NetlinkNestedAttribute::SetNestedHasAValue() {
  has_a_value_ = true;
  return true;
}

bool NetlinkNestedAttribute::InitNestedFromNlAttr(
    AttributeList *list,
    const NetlinkNestedAttribute::NestedData::NestedDataMap &templates,
    const nlattr *const_data) {
  if (templates.size() == 1 && templates.cbegin()->second.is_array) {
    return ParseNestedArray(list, templates.cbegin()->second, const_data);
  } else {
    return ParseNestedStructure(list, templates, const_data);
  }
  return true;
}

// A nested array provides an arbitrary number of children, all of the same
// data type.  Each array element may be a simple type or may be a structure.
//
// static
bool NetlinkNestedAttribute::ParseNestedArray(AttributeList *list,
                                              const NestedData &array_template,
                                              const nlattr *const_data) {
  if (!list) {
    LOG(ERROR) << "NULL |list| parameter";
    return false;
  }
  if (!const_data) {
    LOG(ERROR) << "Null |const_data| parameter";
    return false;
  }
  // Casting away constness since |nla_parse_nested| doesn't mark its input as
  // const even though it doesn't modify this input parameter.
  nlattr *attrs = const_cast<nlattr *>(const_data);

  struct nlattr *attr;
  int remaining;
  nla_for_each_nested_type_corrected(attr, attrs, remaining) {
    string attribute_name = StringPrintf(
        "%s_%d", array_template.attribute_name.c_str(), attr->nla_type);
    AddAttributeToNested(list, array_template.type, attr->nla_type,
                         attribute_name, *attr, array_template);
  }
  return true;
}

// A nested structure provides a fixed set of child attributes (some of
// which may be optional).
// static
bool NetlinkNestedAttribute::ParseNestedStructure(
    AttributeList *list,
    const NetlinkNestedAttribute::NestedData::NestedDataMap &templates,
    const nlattr *const_data) {
  if (!list) {
    LOG(ERROR) << "NULL |list| parameter";
    return false;
  }
  if (templates.empty()) {
    LOG(ERROR) << "|templates| size is zero";
    return false;
  }
  if (!const_data) {
    LOG(ERROR) << "Null |const_data| parameter";
    return false;
  }
  // Casting away constness since |nla_parse_nested| doesn't mark its input as
  // const even though it doesn't modify this input parameter.
  nlattr *attr_data = const_cast<nlattr *>(const_data);

  // |nla_parse_nested| requires an array of |nla_policy|. While an attribute id
  // of zero is illegal, we still need to fill that spot in the policy
  // array so the loop will start at zero.
  size_t largest_attribute = templates.crbegin()->first;
  unique_ptr<nla_policy[]> policy(new nla_policy[largest_attribute + 1]);
  memset(&policy[0], 0, sizeof(nla_policy) * (largest_attribute + 1));
  for (const auto &temp : templates) {
    const size_t &id = temp.first;
    policy[id].type = temp.second.type;
  }

  // |nla_parse_nested| builds an array of |nlattr| from the input message.
  unique_ptr<nlattr *[]> attr(new nlattr *[largest_attribute + 1]);
  if (nla_parse_nested(attr.get(), largest_attribute, attr_data,
                       policy.get())) {
    LOG(ERROR) << "nla_parse_nested failed";
    return false;
  }

  // Note that the attribute id of zero is illegal so we'll start with id=1.
  for (const auto &temp : templates) {
    const size_t &id = temp.first;
    if (attr[id]) {
      AddAttributeToNested(list, policy[id].type, id,
                           temp.second.attribute_name, *attr[id], temp.second);
    }
  }
  return true;
}

// static
void NetlinkNestedAttribute::AddAttributeToNested(
    AttributeList *list, uint16_t type, size_t id, const string &attribute_name,
    const nlattr &attr, const NestedData &nested_template) {
  CHECK(list);
  if (!nested_template.parse_attribute.is_null()) {
    if (!nested_template.parse_attribute.Run(
        list, id, attribute_name,
        ByteString(reinterpret_cast<const char *>(nla_data(&attr)),
                   nla_len(&attr)))) {
      LOG(WARNING) << "Custom attribute parser returned |false| for "
                   << attribute_name << "(" << id << ").";
    }
    return;
  }
  switch (type) {
    case NLA_UNSPEC:
      list->CreateRawAttribute(id, attribute_name.c_str());
      list->SetRawAttributeValue(id, ByteString(
          reinterpret_cast<const char *>(nla_data(&attr)), nla_len(&attr)));
      break;
    case NLA_U8:
      list->CreateU8Attribute(id, attribute_name.c_str());
      list->SetU8AttributeValue(id, NlaGetU8(&attr));
      break;
    case NLA_U16:
      list->CreateU16Attribute(id, attribute_name.c_str());
      list->SetU16AttributeValue(id, NlaGetU16(&attr));
      break;
    case NLA_U32:
      list->CreateU32Attribute(id, attribute_name.c_str());
      list->SetU32AttributeValue(id, NlaGetU32(&attr));
      break;
    case NLA_U64:
      list->CreateU64Attribute(id, attribute_name.c_str());
      list->SetU64AttributeValue(id, NlaGetU64(&attr));
      break;
    case NLA_FLAG:
      list->CreateFlagAttribute(id, attribute_name.c_str());
      list->SetFlagAttributeValue(id, true);
      break;
    case NLA_STRING:
      // Note that nested structure attributes are validated by |validate_nla|
      // which requires a string attribute to have at least 1 character
      // (presumably for the '\0') while the kernel can create an empty string
      // for at least one nested string array attribute type
      // (NL80211_ATTR_SCAN_SSIDS -- the emptyness of the string is exhibited
      // by the value of |nlattr::nla_len|).  This code handles both cases.
      list->CreateStringAttribute(id, attribute_name.c_str());
      if (nla_len(&attr) <= 0) {
        list->SetStringAttributeValue(id, "");
      } else {
        list->SetStringAttributeValue(id, NlaGetString(&attr));
      }
      break;
    case NLA_NESTED:
      {
        if (nested_template.deeper_nesting.empty()) {
          LOG(ERROR) << "No rules for nesting " << attribute_name
                     << ". Ignoring.";
          break;
        }
        list->CreateNestedAttribute(id, attribute_name.c_str());

        // Now, handle the nested data.
        AttributeListRefPtr nested_attribute;
        if (!list->GetNestedAttributeList(id, &nested_attribute) ||
            !nested_attribute) {
          LOG(FATAL) << "Couldn't get attribute " << attribute_name
                     << " which we just created.";
          break;
        }

        if (!InitNestedFromNlAttr(nested_attribute.get(),
                                  nested_template.deeper_nesting,
                                  &attr)) {
          LOG(ERROR) << "Couldn't parse attribute " << attribute_name;
          break;
        }
        list->SetNestedAttributeHasAValue(id);
      }
      break;
    default:
      LOG(ERROR) << "Discarding " << attribute_name
                 << ".  Attribute has unhandled type " << type << ".";
      break;
  }
}

NetlinkNestedAttribute::NestedData::NestedData()
    : type(NLA_UNSPEC), attribute_name("<UNKNOWN>"), is_array(false) {}
NetlinkNestedAttribute::NestedData::NestedData(
    uint16_t type_arg, string attribute_name_arg, bool is_array_arg)
    : type(type_arg), attribute_name(attribute_name_arg),
      is_array(is_array_arg) {}

NetlinkNestedAttribute::NestedData::NestedData(
    uint16_t type_arg, string attribute_name_arg, bool is_array_arg,
    const AttributeParser &parse_attribute_arg)
    : type(type_arg), attribute_name(attribute_name_arg),
      is_array(is_array_arg), parse_attribute(parse_attribute_arg) {}

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
    VLOG(7)  << "Raw attribute " << id_string()
             << " hasn't been set to any value.";
    return false;
  }
  if (output) {
    *output = data_;
  }
  return true;
}

bool NetlinkRawAttribute::SetRawValue(const ByteString new_value) {
  data_ = new_value;
  has_a_value_ = true;
  return true;
}

bool NetlinkRawAttribute::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "Null |output| parameter";
    return false;
  }
  if (!has_a_value_) {
    VLOG(7)  << "Raw attribute " << id_string()
             << " hasn't been set to any value.";
    return false;
  }
  int total_bytes = data_.GetLength();
  const uint8_t *const_data = data_.GetConstData();

  *output = StringPrintf("%d bytes:", total_bytes);
  for (int i = 0; i < total_bytes; ++i) {
    StringAppendF(output, " 0x%02x", const_data[i]);
  }
  return true;
}

ByteString NetlinkRawAttribute::Encode() const {
  return NetlinkAttribute::EncodeGeneric(data_.GetConstData(),
                                         data_.GetLength());
}

NetlinkAttributeGeneric::NetlinkAttributeGeneric(int id)
    : NetlinkRawAttribute(id, "unused-string") {
  StringAppendF(&id_string_, "<UNKNOWN ATTRIBUTE %d>", id);
}

const char *NetlinkAttributeGeneric::id_string() const {
  return id_string_.c_str();
}

}  // namespace shill
