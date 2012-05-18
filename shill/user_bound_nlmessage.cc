// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code is derived from the 'iw' source code.  The copyright and license
// of that code is as follows:
//
// Copyright (c) 2007, 2008  Johannes Berg
// Copyright (c) 2007  Andy Lutomirski
// Copyright (c) 2007  Mike Kershaw
// Copyright (c) 2008-2009  Luis R. Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "shill/user_bound_nlmessage.h"

#include <ctype.h>
#include <endian.h>
#include <errno.h>

#include <netinet/in.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <netlink/attr.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>

#include <iomanip>
#include <string>

#include <base/format_macros.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>

#include "shill/ieee80211.h"
#include "shill/logging.h"
#include "shill/scope_logger.h"

using base::LazyInstance;
using base::StringAppendF;
using base::StringPrintf;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {
LazyInstance<UserBoundNlMessageDataCollector> g_datacollector =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

const uint8_t UserBoundNlMessage::kCommand = 0xff;
const char UserBoundNlMessage::kCommandString[] = "<Unknown Message>";
const char UserBoundNlMessage::kBogusMacAddress[]="XX:XX:XX:XX:XX:XX";

const uint8_t Nl80211Frame::kMinimumFrameByteCount = 26;
const uint8_t Nl80211Frame::kFrameTypeMask = 0xfc;

const uint32_t UserBoundNlMessage::kIllegalMessage = 0xFFFFFFFF;
const int UserBoundNlMessage::kEthernetAddressBytes = 6;
map<uint16_t, string> *UserBoundNlMessage::connect_status_map_ = NULL;

// The nl messages look like this:
//
//       XXXXXXXXXXXXXXXXXXX-nlmsg_total_size-XXXXXXXXXXXXXXXXXXX
//       XXXXXXXXXXXXXXXXXXX-nlmsg_msg_size-XXXXXXXXXXXXXXXXXXX
//               +-- gnhl                        nlmsg_tail(hdr) --+
//               |                               nlmsg_next(hdr) --+
//               v        XXXXXXXXXXXX-nlmsg_len-XXXXXXXXXXXXXX    V
// -----+-----+-+----------------------------------------------+-++----
//  ... |     | |                payload                       | ||
//      |     | +------+-+--------+-+--------------------------+ ||
//      | nl  | |      | |        | |      attribs             | ||
//      | msg |p| genl |p| family |p+------+-+-------+-+-------+p|| ...
//      | hdr |a| msg  |a| header |a| nl   |p| pay   |p|       |a||
//      |     |d| hdr  |d|        |d| attr |a| load  |a| ...   |d||
//      |     | |      | |        | |      |d|       |d|       | ||
// -----+-----+-+----------------------------------------------+-++----
//       ^       ^                   ^        ^
//       |       |                   |        XXXXXXX <-- nla_len(nlattr)
//       |       |                   |        +-- nla_data(nlattr)
//       |       |                   X-nla_total_size-X
//       |       |                   XXXXX-nlmsg_attrlen-XXXXXX
//       |       +-- nlmsg_data(hdr) +-- nlmsg_attrdata()
//       +-- msg = nlmsg_hdr(raw_message)

//
// UserBoundNlMessage
//

UserBoundNlMessage::~UserBoundNlMessage() {
  map<enum nl80211_attrs, nlattr *>::iterator i;
  for (i = attributes_.begin(); i != attributes_.end(); ++i) {
    delete [] reinterpret_cast<uint8_t *>(i->second);
  }
}

bool UserBoundNlMessage::Init(nlattr *tb[NL80211_ATTR_MAX + 1],
                              nlmsghdr *msg) {
  if (!tb) {
    LOG(ERROR) << "Null |tb| parameter";
    return false;
  }

  message_ = msg;

  SLOG(WiFi, 6) << "NL Message " << GetId() << " <===";

  for (int i = 0; i < NL80211_ATTR_MAX + 1; ++i) {
    if (tb[i]) {
      AddAttribute(static_cast<enum nl80211_attrs>(i), tb[i]);
    }
  }

  // Convert integer values provided by libnl (for example, from the
  // NL80211_ATTR_STATUS_CODE or NL80211_ATTR_REASON_CODE attribute) into
  // strings describing the status.
  if (!connect_status_map_) {
    connect_status_map_ = new map<uint16_t, string>;
    (*connect_status_map_)[0] = "Successful";
    (*connect_status_map_)[1] = "Unspecified failure";
    (*connect_status_map_)[2] = "Previous authentication no longer valid";
    (*connect_status_map_)[3] = "Deauthenticated because sending station is "
        "leaving (or has left) the IBSS or ESS";
    (*connect_status_map_)[7] = "Class 3 frame received from non-authenticated "
        "station";
    (*connect_status_map_)[10] = "Cannot support all requested capabilities in "
        "the capability information field";
    (*connect_status_map_)[11] = "Reassociation denied due to inability to "
        "confirm that association exists";
    (*connect_status_map_)[12] = "Association denied due to reason outside the "
        "scope of this standard";
    (*connect_status_map_)[13] = "Responding station does not support the "
        "specified authentication algorithm";
    (*connect_status_map_)[14] = "Received an authentication frame with "
        "authentication transaction sequence number out of expected sequence";
    (*connect_status_map_)[15] = "Authentication rejected because of challenge "
        "failure";
    (*connect_status_map_)[16] = "Authentication rejected due to timeout "
        "waiting for next frame in sequence";
    (*connect_status_map_)[17] = "Association denied because AP is unable to "
        "handle additional associated STA";
    (*connect_status_map_)[18] = "Association denied due to requesting station "
        "not supporting all of the data rates in the BSSBasicRateSet parameter";
    (*connect_status_map_)[19] = "Association denied due to requesting station "
        "not supporting the short preamble option";
    (*connect_status_map_)[20] = "Association denied due to requesting station "
        "not supporting the PBCC modulation option";
    (*connect_status_map_)[21] = "Association denied due to requesting station "
        "not supporting the channel agility option";
    (*connect_status_map_)[22] = "Association request rejected because "
        "Spectrum Management capability is required";
    (*connect_status_map_)[23] = "Association request rejected because the "
        "information in the Power Capability element is unacceptable";
    (*connect_status_map_)[24] = "Association request rejected because the "
        "information in the Supported Channels element is unacceptable";
    (*connect_status_map_)[25] = "Association request rejected due to "
        "requesting station not supporting the short slot time option";
    (*connect_status_map_)[26] = "Association request rejected due to "
        "requesting station not supporting the ER-PBCC modulation option";
    (*connect_status_map_)[27] = "Association denied due to requesting STA not "
        "supporting HT features";
    (*connect_status_map_)[28] = "R0KH Unreachable";
    (*connect_status_map_)[29] = "Association denied because the requesting "
        "STA does not support the PCO transition required by the AP";
    (*connect_status_map_)[30] = "Association request rejected temporarily; "
        "try again later";
    (*connect_status_map_)[31] = "Robust Management frame policy violation";
    (*connect_status_map_)[32] = "Unspecified, QoS related failure";
    (*connect_status_map_)[33] = "Association denied due to QAP having "
        "insufficient bandwidth to handle another QSTA";
    (*connect_status_map_)[34] = "Association denied due to poor channel "
        "conditions";
    (*connect_status_map_)[35] = "Association (with QBSS) denied due to "
        "requesting station not supporting the QoS facility";
    (*connect_status_map_)[37] = "The request has been declined";
    (*connect_status_map_)[38] = "The request has not been successful as one "
        "or more parameters have invalid values";
    (*connect_status_map_)[39] = "The TS has not been created because the "
        "request cannot be honored. However, a suggested Tspec is provided so "
        "that the initiating QSTA may attempt to send another TS with the "
        "suggested changes to the TSpec";
    (*connect_status_map_)[40] = "Invalid Information Element";
    (*connect_status_map_)[41] = "Group Cipher is not valid";
    (*connect_status_map_)[42] = "Pairwise Cipher is not valid";
    (*connect_status_map_)[43] = "AKMP is not valid";
    (*connect_status_map_)[44] = "Unsupported RSN IE version";
    (*connect_status_map_)[45] = "Invalid RSN IE Capabilities";
    (*connect_status_map_)[46] = "Cipher suite is rejected per security policy";
    (*connect_status_map_)[47] = "The TS has not been created. However, the HC "
        "may be capable of creating a TS, in response to a request, after the "
        "time indicated in the TS Delay element";
    (*connect_status_map_)[48] = "Direct link is not allowed in the BSS by "
        "policy";
    (*connect_status_map_)[49] = "Destination STA is not present within this "
        "QBSS";
    (*connect_status_map_)[50] = "The destination STA is not a QSTA";
    (*connect_status_map_)[51] = "Association denied because Listen Interval "
        "is too large";
    (*connect_status_map_)[52] = "Invalid Fast BSS Transition Action Frame "
        "Count";
    (*connect_status_map_)[53] = "Invalid PMKID";
    (*connect_status_map_)[54] = "Invalid MDIE";
    (*connect_status_map_)[55] = "Invalid FTIE";
  }

  return true;
}

UserBoundNlMessage::AttributeNameIterator*
    UserBoundNlMessage::GetAttributeNameIterator() const {
  UserBoundNlMessage::AttributeNameIterator *iter =
      new UserBoundNlMessage::AttributeNameIterator(attributes_);
  return iter;
}

// Return true if the attribute is in our map, regardless of the value of
// the attribute, itself.
bool UserBoundNlMessage::AttributeExists(enum nl80211_attrs name) const {
  return ContainsKey(attributes_, name);
}

uint32_t UserBoundNlMessage::GetId() const {
  if (!message_) {
    return kIllegalMessage;
  }
  return message_->nlmsg_seq;
}

enum UserBoundNlMessage::Type
    UserBoundNlMessage::GetAttributeType(enum nl80211_attrs name) const {
  const nlattr *attr = GetAttribute(name);
  if (!attr) {
    return kTypeError;
  }

  switch (nla_type(attr)) {
    case NLA_UNSPEC: return kTypeUnspecified;
    case NLA_U8: return kTypeU8;
    case NLA_U16: return kTypeU16;
    case NLA_U32: return kTypeU32;
    case NLA_U64: return kTypeU64;
    case NLA_STRING: return kTypeString;
    case NLA_FLAG: return kTypeFlag;
    case NLA_MSECS: return kTypeMsecs;
    case NLA_NESTED: return kTypeNested;
    default: return kTypeError;
  }

  return kTypeError;
}

string UserBoundNlMessage::GetAttributeTypeString(enum nl80211_attrs name)
    const {
  switch (GetAttributeType(name)) {
    case kTypeUnspecified: return "Unspecified Type"; break;
    case kTypeU8: return "uint8_t"; break;
    case kTypeU16: return "uint16_t"; break;
    case kTypeU32: return "uint32_t"; break;
    case kTypeU64: return "uint64_t"; break;
    case kTypeString: return "String"; break;
    case kTypeFlag: return "Flag"; break;
    case kTypeMsecs: return "MSec Type"; break;
    case kTypeNested: return "Nested Type"; break;
    case kTypeError: return "ERROR TYPE"; break;
    default: return "Funky Type"; break;
  }
}

// Returns the raw attribute data but not the header.
bool UserBoundNlMessage::GetRawAttributeData(enum nl80211_attrs name,
                                             void **value,
                                             int *length) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  const nlattr *attr = GetAttribute(name);
  if (!attr) {
    *value = NULL;
    return false;
  }

  if (length) {
    *length = nla_len(attr);
  }
  *value = nla_data(attr);
  return true;
}

bool UserBoundNlMessage::GetStringAttribute(enum nl80211_attrs name,
                                            string *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  const nlattr *attr = GetAttribute(name);
  if (!attr) {
    return false;
  }

  value->assign(nla_get_string(const_cast<nlattr *>(attr)));
  return true;
}

bool UserBoundNlMessage::GetU8Attribute(enum nl80211_attrs name,
                                        uint8_t *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  const nlattr *attr = GetAttribute(name);
  if (!attr) {
    return false;
  }

  *value = nla_get_u8(const_cast<nlattr *>(attr));
  return true;
}

bool UserBoundNlMessage::GetU16Attribute(enum nl80211_attrs name,
                                         uint16_t *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  const nlattr *attr = GetAttribute(name);
  if (!attr) {
    return false;
  }

  *value = nla_get_u16(const_cast<nlattr *>(attr));
  return true;
}

bool UserBoundNlMessage::GetU32Attribute(enum nl80211_attrs name,
                                         uint32_t *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  const nlattr *attr = GetAttribute(name);
  if (!attr) {
    return false;
  }

  *value = nla_get_u32(const_cast<nlattr *>(attr));
  return true;
}

bool UserBoundNlMessage::GetU64Attribute(enum nl80211_attrs name,
                                         uint64_t *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  const nlattr *attr = GetAttribute(name);
  if (!attr) {
    return false;
  }

  *value = nla_get_u64(const_cast<nlattr *>(attr));
  return true;
}

// Helper function to provide a string for a MAC address.
bool UserBoundNlMessage::GetMacAttributeString(enum nl80211_attrs name,
                                               string *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  void *rawdata_void = NULL;
  if (!GetRawAttributeData(name, &rawdata_void, NULL)) {
    value->assign(kBogusMacAddress);
    return false;
  }
  const uint8_t *rawdata = reinterpret_cast<const uint8_t *>(rawdata_void);
  value->assign(StringFromMacAddress(rawdata));

  return true;
}

// Helper function to provide a string for NL80211_ATTR_SCAN_FREQUENCIES.
bool UserBoundNlMessage::GetScanFrequenciesAttribute(
    enum nl80211_attrs name, vector<uint32_t> *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  value->clear();
  if (AttributeExists(name)) {
    void *rawdata = NULL;
    int len = 0;
    if (GetRawAttributeData(name, &rawdata, &len) && rawdata) {
      nlattr *nst = NULL;
      nlattr *attr_data = reinterpret_cast<nlattr *>(rawdata);
      int rem_nst;

      nla_for_each_attr(nst, attr_data, len, rem_nst) {
        value->push_back(nla_get_u32(nst));
      }
    }
    return true;
  }

  return false;
}

// Helper function to provide a string for NL80211_ATTR_SCAN_SSIDS.
bool UserBoundNlMessage::GetScanSsidsAttribute(
    enum nl80211_attrs name, vector<string> *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  if (AttributeExists(name)) {
    void *rawdata = NULL;
    int len = 0;
    if (GetRawAttributeData(name, &rawdata, &len) && rawdata) {
      nlattr *nst = NULL;
      nlattr *data = reinterpret_cast<nlattr *>(rawdata);
      int rem_nst;

      nla_for_each_attr(nst, data, len, rem_nst) {
        value->push_back(StringFromSsid(nla_len(nst),
                                        reinterpret_cast<const uint8_t *>(
                                          nla_data(nst))).c_str());
      }
    }
    return true;
  }

  return false;
}

bool UserBoundNlMessage::GetAttributeString(nl80211_attrs name,
                                            string *param) const {
  if (!param) {
    LOG(ERROR) << "Null |param| parameter";
    return false;
  }

  switch (GetAttributeType(name)) {
    case kTypeU8: {
      uint8_t value;
      if (!GetU8Attribute(name, &value))
        return false;
      *param = StringPrintf("%u", value);
      break;
    }
    case kTypeU16: {
      uint16_t value;
      if (!GetU16Attribute(name, &value))
        return false;
      *param = StringPrintf("%u", value);
      break;
    }
    case kTypeU32: {
      uint32_t value;
      if (!GetU32Attribute(name, &value))
        return false;
      *param = StringPrintf("%" PRIu32, value);
      break;
    }
    case kTypeU64: {
      uint64_t value;
      if (!GetU64Attribute(name, &value))
        return false;
      *param = StringPrintf("%" PRIu64, value);
      break;
    }
    case kTypeString:
      if (!GetStringAttribute(name, param))
        return false;
      break;

    default:
      return false;
  }
  return true;
}

string UserBoundNlMessage::RawToString(enum nl80211_attrs name) const {
  string output = " === RAW: ";

  const nlattr *attr = GetAttribute(name);
  if (!attr) {
    output.append("<NULL> ===");
    return output;
  }

  const char *typestring = NULL;
  switch (nla_type(attr)) {
    case NLA_UNSPEC: typestring = "NLA_UNSPEC"; break;
    case NLA_U8: typestring = "NLA_U8"; break;
    case NLA_U16: typestring = "NLA_U16"; break;
    case NLA_U32: typestring = "NLA_U32"; break;
    case NLA_U64: typestring = "NLA_U64"; break;
    case NLA_STRING: typestring = "NLA_STRING"; break;
    case NLA_FLAG: typestring = "NLA_FLAG"; break;
    case NLA_MSECS: typestring = "NLA_MSECS"; break;
    case NLA_NESTED: typestring = "NLA_NESTED"; break;
    default: typestring = "<UNKNOWN>"; break;
  }

  uint16_t length = nla_len(attr);
  uint16_t type = nla_type(attr);
  StringAppendF(&output, "len=%u type=(%u)=%s", length, type, typestring);

  const uint8_t *const_data
      = reinterpret_cast<const uint8_t *>(nla_data(attr));

  output.append(" DATA: ");
  for (int i =0 ; i < length; ++i) {
    StringAppendF(&output, "[%d]=%02x ",
                  i, *(reinterpret_cast<const uint8_t *>(const_data)+i));
  }
  output.append(" ==== ");
  return output;
}

// static
string UserBoundNlMessage::StringFromAttributeName(enum nl80211_attrs name) {
  switch (name) {
    case NL80211_ATTR_UNSPEC:
      return "NL80211_ATTR_UNSPEC"; break;
    case NL80211_ATTR_WIPHY:
      return "NL80211_ATTR_WIPHY"; break;
    case NL80211_ATTR_WIPHY_NAME:
      return "NL80211_ATTR_WIPHY_NAME"; break;
    case NL80211_ATTR_IFINDEX:
      return "NL80211_ATTR_IFINDEX"; break;
    case NL80211_ATTR_IFNAME:
      return "NL80211_ATTR_IFNAME"; break;
    case NL80211_ATTR_IFTYPE:
      return "NL80211_ATTR_IFTYPE"; break;
    case NL80211_ATTR_MAC:
      return "NL80211_ATTR_MAC"; break;
    case NL80211_ATTR_KEY_DATA:
      return "NL80211_ATTR_KEY_DATA"; break;
    case NL80211_ATTR_KEY_IDX:
      return "NL80211_ATTR_KEY_IDX"; break;
    case NL80211_ATTR_KEY_CIPHER:
      return "NL80211_ATTR_KEY_CIPHER"; break;
    case NL80211_ATTR_KEY_SEQ:
      return "NL80211_ATTR_KEY_SEQ"; break;
    case NL80211_ATTR_KEY_DEFAULT:
      return "NL80211_ATTR_KEY_DEFAULT"; break;
    case NL80211_ATTR_BEACON_INTERVAL:
      return "NL80211_ATTR_BEACON_INTERVAL"; break;
    case NL80211_ATTR_DTIM_PERIOD:
      return "NL80211_ATTR_DTIM_PERIOD"; break;
    case NL80211_ATTR_BEACON_HEAD:
      return "NL80211_ATTR_BEACON_HEAD"; break;
    case NL80211_ATTR_BEACON_TAIL:
      return "NL80211_ATTR_BEACON_TAIL"; break;
    case NL80211_ATTR_STA_AID:
      return "NL80211_ATTR_STA_AID"; break;
    case NL80211_ATTR_STA_FLAGS:
      return "NL80211_ATTR_STA_FLAGS"; break;
    case NL80211_ATTR_STA_LISTEN_INTERVAL:
      return "NL80211_ATTR_STA_LISTEN_INTERVAL"; break;
    case NL80211_ATTR_STA_SUPPORTED_RATES:
      return "NL80211_ATTR_STA_SUPPORTED_RATES"; break;
    case NL80211_ATTR_STA_VLAN:
      return "NL80211_ATTR_STA_VLAN"; break;
    case NL80211_ATTR_STA_INFO:
      return "NL80211_ATTR_STA_INFO"; break;
    case NL80211_ATTR_WIPHY_BANDS:
      return "NL80211_ATTR_WIPHY_BANDS"; break;
    case NL80211_ATTR_MNTR_FLAGS:
      return "NL80211_ATTR_MNTR_FLAGS"; break;
    case NL80211_ATTR_MESH_ID:
      return "NL80211_ATTR_MESH_ID"; break;
    case NL80211_ATTR_STA_PLINK_ACTION:
      return "NL80211_ATTR_STA_PLINK_ACTION"; break;
    case NL80211_ATTR_MPATH_NEXT_HOP:
      return "NL80211_ATTR_MPATH_NEXT_HOP"; break;
    case NL80211_ATTR_MPATH_INFO:
      return "NL80211_ATTR_MPATH_INFO"; break;
    case NL80211_ATTR_BSS_CTS_PROT:
      return "NL80211_ATTR_BSS_CTS_PROT"; break;
    case NL80211_ATTR_BSS_SHORT_PREAMBLE:
      return "NL80211_ATTR_BSS_SHORT_PREAMBLE"; break;
    case NL80211_ATTR_BSS_SHORT_SLOT_TIME:
      return "NL80211_ATTR_BSS_SHORT_SLOT_TIME"; break;
    case NL80211_ATTR_HT_CAPABILITY:
      return "NL80211_ATTR_HT_CAPABILITY"; break;
    case NL80211_ATTR_SUPPORTED_IFTYPES:
      return "NL80211_ATTR_SUPPORTED_IFTYPES"; break;
    case NL80211_ATTR_REG_ALPHA2:
      return "NL80211_ATTR_REG_ALPHA2"; break;
    case NL80211_ATTR_REG_RULES:
      return "NL80211_ATTR_REG_RULES"; break;
    case NL80211_ATTR_MESH_CONFIG:
      return "NL80211_ATTR_MESH_CONFIG"; break;
    case NL80211_ATTR_BSS_BASIC_RATES:
      return "NL80211_ATTR_BSS_BASIC_RATES"; break;
    case NL80211_ATTR_WIPHY_TXQ_PARAMS:
      return "NL80211_ATTR_WIPHY_TXQ_PARAMS"; break;
    case NL80211_ATTR_WIPHY_FREQ:
      return "NL80211_ATTR_WIPHY_FREQ"; break;
    case NL80211_ATTR_WIPHY_CHANNEL_TYPE:
      return "NL80211_ATTR_WIPHY_CHANNEL_TYPE"; break;
    case NL80211_ATTR_KEY_DEFAULT_MGMT:
      return "NL80211_ATTR_KEY_DEFAULT_MGMT"; break;
    case NL80211_ATTR_MGMT_SUBTYPE:
      return "NL80211_ATTR_MGMT_SUBTYPE"; break;
    case NL80211_ATTR_IE:
      return "NL80211_ATTR_IE"; break;
    case NL80211_ATTR_MAX_NUM_SCAN_SSIDS:
      return "NL80211_ATTR_MAX_NUM_SCAN_SSIDS"; break;
    case NL80211_ATTR_SCAN_FREQUENCIES:
      return "NL80211_ATTR_SCAN_FREQUENCIES"; break;
    case NL80211_ATTR_SCAN_SSIDS:
      return "NL80211_ATTR_SCAN_SSIDS"; break;
    case NL80211_ATTR_GENERATION:
      return "NL80211_ATTR_GENERATION"; break;
    case NL80211_ATTR_BSS:
      return "NL80211_ATTR_BSS"; break;
    case NL80211_ATTR_REG_INITIATOR:
      return "NL80211_ATTR_REG_INITIATOR"; break;
    case NL80211_ATTR_REG_TYPE:
      return "NL80211_ATTR_REG_TYPE"; break;
    case NL80211_ATTR_SUPPORTED_COMMANDS:
      return "NL80211_ATTR_SUPPORTED_COMMANDS"; break;
    case NL80211_ATTR_FRAME:
      return "NL80211_ATTR_FRAME"; break;
    case NL80211_ATTR_SSID:
      return "NL80211_ATTR_SSID"; break;
    case NL80211_ATTR_AUTH_TYPE:
      return "NL80211_ATTR_AUTH_TYPE"; break;
    case NL80211_ATTR_REASON_CODE:
      return "NL80211_ATTR_REASON_CODE"; break;
    case NL80211_ATTR_KEY_TYPE:
      return "NL80211_ATTR_KEY_TYPE"; break;
    case NL80211_ATTR_MAX_SCAN_IE_LEN:
      return "NL80211_ATTR_MAX_SCAN_IE_LEN"; break;
    case NL80211_ATTR_CIPHER_SUITES:
      return "NL80211_ATTR_CIPHER_SUITES"; break;
    case NL80211_ATTR_FREQ_BEFORE:
      return "NL80211_ATTR_FREQ_BEFORE"; break;
    case NL80211_ATTR_FREQ_AFTER:
      return "NL80211_ATTR_FREQ_AFTER"; break;
    case NL80211_ATTR_FREQ_FIXED:
      return "NL80211_ATTR_FREQ_FIXED"; break;
    case NL80211_ATTR_WIPHY_RETRY_SHORT:
      return "NL80211_ATTR_WIPHY_RETRY_SHORT"; break;
    case NL80211_ATTR_WIPHY_RETRY_LONG:
      return "NL80211_ATTR_WIPHY_RETRY_LONG"; break;
    case NL80211_ATTR_WIPHY_FRAG_THRESHOLD:
      return "NL80211_ATTR_WIPHY_FRAG_THRESHOLD"; break;
    case NL80211_ATTR_WIPHY_RTS_THRESHOLD:
      return "NL80211_ATTR_WIPHY_RTS_THRESHOLD"; break;
    case NL80211_ATTR_TIMED_OUT:
      return "NL80211_ATTR_TIMED_OUT"; break;
    case NL80211_ATTR_USE_MFP:
      return "NL80211_ATTR_USE_MFP"; break;
    case NL80211_ATTR_STA_FLAGS2:
      return "NL80211_ATTR_STA_FLAGS2"; break;
    case NL80211_ATTR_CONTROL_PORT:
      return "NL80211_ATTR_CONTROL_PORT"; break;
    case NL80211_ATTR_TESTDATA:
      return "NL80211_ATTR_TESTDATA"; break;
    case NL80211_ATTR_PRIVACY:
      return "NL80211_ATTR_PRIVACY"; break;
    case NL80211_ATTR_DISCONNECTED_BY_AP:
      return "NL80211_ATTR_DISCONNECTED_BY_AP"; break;
    case NL80211_ATTR_STATUS_CODE:
      return "NL80211_ATTR_STATUS_CODE"; break;
    case NL80211_ATTR_CIPHER_SUITES_PAIRWISE:
      return "NL80211_ATTR_CIPHER_SUITES_PAIRWISE"; break;
    case NL80211_ATTR_CIPHER_SUITE_GROUP:
      return "NL80211_ATTR_CIPHER_SUITE_GROUP"; break;
    case NL80211_ATTR_WPA_VERSIONS:
      return "NL80211_ATTR_WPA_VERSIONS"; break;
    case NL80211_ATTR_AKM_SUITES:
      return "NL80211_ATTR_AKM_SUITES"; break;
    case NL80211_ATTR_REQ_IE:
      return "NL80211_ATTR_REQ_IE"; break;
    case NL80211_ATTR_RESP_IE:
      return "NL80211_ATTR_RESP_IE"; break;
    case NL80211_ATTR_PREV_BSSID:
      return "NL80211_ATTR_PREV_BSSID"; break;
    case NL80211_ATTR_KEY:
      return "NL80211_ATTR_KEY"; break;
    case NL80211_ATTR_KEYS:
      return "NL80211_ATTR_KEYS"; break;
    case NL80211_ATTR_PID:
      return "NL80211_ATTR_PID"; break;
    case NL80211_ATTR_4ADDR:
      return "NL80211_ATTR_4ADDR"; break;
    case NL80211_ATTR_SURVEY_INFO:
      return "NL80211_ATTR_SURVEY_INFO"; break;
    case NL80211_ATTR_PMKID:
      return "NL80211_ATTR_PMKID"; break;
    case NL80211_ATTR_MAX_NUM_PMKIDS:
      return "NL80211_ATTR_MAX_NUM_PMKIDS"; break;
    case NL80211_ATTR_DURATION:
      return "NL80211_ATTR_DURATION"; break;
    case NL80211_ATTR_COOKIE:
      return "NL80211_ATTR_COOKIE"; break;
    case NL80211_ATTR_WIPHY_COVERAGE_CLASS:
      return "NL80211_ATTR_WIPHY_COVERAGE_CLASS"; break;
    case NL80211_ATTR_TX_RATES:
      return "NL80211_ATTR_TX_RATES"; break;
    case NL80211_ATTR_FRAME_MATCH:
      return "NL80211_ATTR_FRAME_MATCH"; break;
    case NL80211_ATTR_ACK:
      return "NL80211_ATTR_ACK"; break;
    case NL80211_ATTR_PS_STATE:
      return "NL80211_ATTR_PS_STATE"; break;
    case NL80211_ATTR_CQM:
      return "NL80211_ATTR_CQM"; break;
    case NL80211_ATTR_LOCAL_STATE_CHANGE:
      return "NL80211_ATTR_LOCAL_STATE_CHANGE"; break;
    case NL80211_ATTR_AP_ISOLATE:
      return "NL80211_ATTR_AP_ISOLATE"; break;
    case NL80211_ATTR_WIPHY_TX_POWER_SETTING:
      return "NL80211_ATTR_WIPHY_TX_POWER_SETTING"; break;
    case NL80211_ATTR_WIPHY_TX_POWER_LEVEL:
      return "NL80211_ATTR_WIPHY_TX_POWER_LEVEL"; break;
    case NL80211_ATTR_TX_FRAME_TYPES:
      return "NL80211_ATTR_TX_FRAME_TYPES"; break;
    case NL80211_ATTR_RX_FRAME_TYPES:
      return "NL80211_ATTR_RX_FRAME_TYPES"; break;
    case NL80211_ATTR_FRAME_TYPE:
      return "NL80211_ATTR_FRAME_TYPE"; break;
    case NL80211_ATTR_CONTROL_PORT_ETHERTYPE:
      return "NL80211_ATTR_CONTROL_PORT_ETHERTYPE"; break;
    case NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT:
      return "NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT"; break;
    case NL80211_ATTR_SUPPORT_IBSS_RSN:
      return "NL80211_ATTR_SUPPORT_IBSS_RSN"; break;
    case NL80211_ATTR_WIPHY_ANTENNA_TX:
      return "NL80211_ATTR_WIPHY_ANTENNA_TX"; break;
    case NL80211_ATTR_WIPHY_ANTENNA_RX:
      return "NL80211_ATTR_WIPHY_ANTENNA_RX"; break;
    case NL80211_ATTR_MCAST_RATE:
      return "NL80211_ATTR_MCAST_RATE"; break;
    case NL80211_ATTR_OFFCHANNEL_TX_OK:
      return "NL80211_ATTR_OFFCHANNEL_TX_OK"; break;
    case NL80211_ATTR_BSS_HT_OPMODE:
      return "NL80211_ATTR_BSS_HT_OPMODE"; break;
    case NL80211_ATTR_KEY_DEFAULT_TYPES:
      return "NL80211_ATTR_KEY_DEFAULT_TYPES"; break;
    case NL80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION:
      return "NL80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION"; break;
    case NL80211_ATTR_MESH_SETUP:
      return "NL80211_ATTR_MESH_SETUP"; break;
    case NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX:
      return "NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX"; break;
    case NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX:
      return "NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX"; break;
    case NL80211_ATTR_SUPPORT_MESH_AUTH:
      return "NL80211_ATTR_SUPPORT_MESH_AUTH"; break;
    case NL80211_ATTR_STA_PLINK_STATE:
      return "NL80211_ATTR_STA_PLINK_STATE"; break;
    case NL80211_ATTR_WOWLAN_TRIGGERS:
      return "NL80211_ATTR_WOWLAN_TRIGGERS"; break;
    case NL80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED:
      return "NL80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED"; break;
    case NL80211_ATTR_SCHED_SCAN_INTERVAL:
      return "NL80211_ATTR_SCHED_SCAN_INTERVAL"; break;
    case NL80211_ATTR_INTERFACE_COMBINATIONS:
      return "NL80211_ATTR_INTERFACE_COMBINATIONS"; break;
    case NL80211_ATTR_SOFTWARE_IFTYPES:
      return "NL80211_ATTR_SOFTWARE_IFTYPES"; break;
    case NL80211_ATTR_REKEY_DATA:
      return "NL80211_ATTR_REKEY_DATA"; break;
    case NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS:
      return "NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS"; break;
    case NL80211_ATTR_MAX_SCHED_SCAN_IE_LEN:
      return "NL80211_ATTR_MAX_SCHED_SCAN_IE_LEN"; break;
    case NL80211_ATTR_SCAN_SUPP_RATES:
      return "NL80211_ATTR_SCAN_SUPP_RATES"; break;
    case NL80211_ATTR_HIDDEN_SSID:
      return "NL80211_ATTR_HIDDEN_SSID"; break;
    case NL80211_ATTR_IE_PROBE_RESP:
      return "NL80211_ATTR_IE_PROBE_RESP"; break;
    case NL80211_ATTR_IE_ASSOC_RESP:
      return "NL80211_ATTR_IE_ASSOC_RESP"; break;
    case NL80211_ATTR_STA_WME:
      return "NL80211_ATTR_STA_WME"; break;
    case NL80211_ATTR_SUPPORT_AP_UAPSD:
      return "NL80211_ATTR_SUPPORT_AP_UAPSD"; break;
    case NL80211_ATTR_ROAM_SUPPORT:
      return "NL80211_ATTR_ROAM_SUPPORT"; break;
    case NL80211_ATTR_SCHED_SCAN_MATCH:
      return "NL80211_ATTR_SCHED_SCAN_MATCH"; break;
    case NL80211_ATTR_MAX_MATCH_SETS:
      return "NL80211_ATTR_MAX_MATCH_SETS"; break;
    case NL80211_ATTR_PMKSA_CANDIDATE:
      return "NL80211_ATTR_PMKSA_CANDIDATE"; break;
    case NL80211_ATTR_TX_NO_CCK_RATE:
      return "NL80211_ATTR_TX_NO_CCK_RATE"; break;
    case NL80211_ATTR_TDLS_ACTION:
      return "NL80211_ATTR_TDLS_ACTION"; break;
    case NL80211_ATTR_TDLS_DIALOG_TOKEN:
      return "NL80211_ATTR_TDLS_DIALOG_TOKEN"; break;
    case NL80211_ATTR_TDLS_OPERATION:
      return "NL80211_ATTR_TDLS_OPERATION"; break;
    case NL80211_ATTR_TDLS_SUPPORT:
      return "NL80211_ATTR_TDLS_SUPPORT"; break;
    case NL80211_ATTR_TDLS_EXTERNAL_SETUP:
      return "NL80211_ATTR_TDLS_EXTERNAL_SETUP"; break;
    case NL80211_ATTR_DEVICE_AP_SME:
      return "NL80211_ATTR_DEVICE_AP_SME"; break;
    case NL80211_ATTR_DONT_WAIT_FOR_ACK:
      return "NL80211_ATTR_DONT_WAIT_FOR_ACK"; break;
    case NL80211_ATTR_FEATURE_FLAGS:
      return "NL80211_ATTR_FEATURE_FLAGS"; break;
    case NL80211_ATTR_PROBE_RESP_OFFLOAD:
      return "NL80211_ATTR_PROBE_RESP_OFFLOAD"; break;
    case NL80211_ATTR_PROBE_RESP:
      return "NL80211_ATTR_PROBE_RESP"; break;
    case NL80211_ATTR_DFS_REGION:
      return "NL80211_ATTR_DFS_REGION"; break;
    case NL80211_ATTR_DISABLE_HT:
      return "NL80211_ATTR_DISABLE_HT"; break;
    case NL80211_ATTR_HT_CAPABILITY_MASK:
      return "NL80211_ATTR_HT_CAPABILITY_MASK"; break;
    case NL80211_ATTR_NOACK_MAP:
      return "NL80211_ATTR_NOACK_MAP"; break;
    case NL80211_ATTR_INACTIVITY_TIMEOUT:
      return "NL80211_ATTR_INACTIVITY_TIMEOUT"; break;
    case NL80211_ATTR_RX_SIGNAL_DBM:
      return "NL80211_ATTR_RX_SIGNAL_DBM"; break;
    case NL80211_ATTR_BG_SCAN_PERIOD:
      return "NL80211_ATTR_BG_SCAN_PERIOD"; break;
    default:
      return "<UNKNOWN>"; break;
  }
}

// Protected members.

// Duplicate attribute data, store in map indexed on |name|.
bool UserBoundNlMessage::AddAttribute(enum nl80211_attrs name,
                                      nlattr *data) {
  if (ContainsKey(attributes_, name)) {
    LOG(ERROR) << "Already have attribute name " << name;
    return false;
  }

  if ((!data) || (nla_total_size(nla_len(data)) == 0)) {
    attributes_[name] = NULL;
  } else {
    nlattr *newdata = reinterpret_cast<nlattr *>(
        new uint8_t[nla_total_size(nla_len(data))]);
    memcpy(newdata, data, nla_total_size(nla_len(data)));
    attributes_[name] = newdata;
  }
  return true;
}

const nlattr *UserBoundNlMessage::GetAttribute(enum nl80211_attrs name)
    const {
  map<enum nl80211_attrs, nlattr *>::const_iterator match;
  match = attributes_.find(name);
  // This method may be called to explore the existence of the attribute so
  // we'll not emit an error if it's not found.
  if (match == attributes_.end()) {
    return NULL;
  }
  return match->second;
}

string UserBoundNlMessage::GetHeaderString() const {
  char ifname[IF_NAMESIZE] = "";
  uint32_t ifindex = UINT32_MAX;
  bool ifindex_exists = GetU32Attribute(NL80211_ATTR_IFINDEX, &ifindex);

  uint32_t wifi = UINT32_MAX;
  bool wifi_exists = GetU32Attribute(NL80211_ATTR_WIPHY, &wifi);

  string output;
  if (ifindex_exists && wifi_exists) {
    StringAppendF(&output, "%s (phy #%" PRIu32 "): ",
                  (if_indextoname(ifindex, ifname) ? ifname : "<unknown>"),
                  wifi);
  } else if (ifindex_exists) {
    StringAppendF(&output, "%s: ",
                  (if_indextoname(ifindex, ifname) ? ifname : "<unknown>"));
  } else if (wifi_exists) {
    StringAppendF(&output, "phy #%" PRIu32 "u: ", wifi);
  }

  return output;
}

string UserBoundNlMessage::StringFromFrame(enum nl80211_attrs attr_name) const {
  string output;

  void *rawdata = NULL;
  int frame_byte_count = 0;
  if (GetRawAttributeData(attr_name, &rawdata, &frame_byte_count) && rawdata) {
    const uint8_t *frame_data = reinterpret_cast<const uint8_t *>(rawdata);

    Nl80211Frame frame(frame_data, frame_byte_count);
    frame.ToString(&output);
  } else {
    output.append(" [no frame]");
  }

  return output;
}

// static
string UserBoundNlMessage::StringFromKeyType(nl80211_key_type key_type) {
  switch (key_type) {
  case NL80211_KEYTYPE_GROUP:
    return "Group";
  case NL80211_KEYTYPE_PAIRWISE:
    return "Pairwise";
  case NL80211_KEYTYPE_PEERKEY:
    return "PeerKey";
  default:
    return "<Unknown Key Type>";
  }
}

// static
string UserBoundNlMessage::StringFromMacAddress(const uint8_t *arg) {
  string output;

  if (!arg) {
    output = kBogusMacAddress;
    LOG(ERROR) << "|arg| parameter is NULL.";
  } else {
    StringAppendF(&output, "%02x", arg[0]);

    for (int i = 1; i < kEthernetAddressBytes ; ++i) {
      StringAppendF(&output, ":%02x", arg[i]);
    }
  }
  return output;
}

// static
string UserBoundNlMessage::StringFromRegInitiator(__u8 initiator) {
  switch (initiator) {
  case NL80211_REGDOM_SET_BY_CORE:
    return "the wireless core upon initialization";
  case NL80211_REGDOM_SET_BY_USER:
    return "a user";
  case NL80211_REGDOM_SET_BY_DRIVER:
    return "a driver";
  case NL80211_REGDOM_SET_BY_COUNTRY_IE:
    return "a country IE";
  default:
    return "<Unknown Reg Initiator>";
  }
}

// static
string UserBoundNlMessage::StringFromSsid(const uint8_t len,
                                          const uint8_t *data) {
  string output;
  if (!data) {
    StringAppendF(&output, "<Error from %s, NULL parameter>", __func__);
    LOG(ERROR) << "|data| parameter is NULL.";
    return output;
  }

  for (int i = 0; i < len; ++i) {
    if (data[i] == ' ')
      output.append(" ");
    else if (isprint(data[i]))
      StringAppendF(&output, "%c", static_cast<char>(data[i]));
    else
      StringAppendF(&output, "\\x%2x", data[i]);
  }

  return output;
}

// static
string UserBoundNlMessage::StringFromStatus(uint16_t status) {
  map<uint16_t, string>::const_iterator match;
  match = connect_status_map_->find(status);
  if (match == connect_status_map_->end()) {
    string output;
    StringAppendF(&output, "<Unknown Status:%u>", status);
    return output;
  }
  return match->second;
}

Nl80211Frame::Nl80211Frame(const uint8_t *raw_frame, int frame_byte_count) :
    frame_type_(kIllegalFrameType), status_(0), frame_(0), byte_count_(0) {
  if (raw_frame == NULL)
    return;

  frame_ = new uint8_t[frame_byte_count];
  memcpy(frame_, raw_frame, frame_byte_count);
  byte_count_ = frame_byte_count;
  const IEEE_80211::ieee80211_frame *frame =
      reinterpret_cast<const IEEE_80211::ieee80211_frame *>(frame_);

  // Now, let's populate the other stuff.
  if (frame_byte_count >= kMinimumFrameByteCount) {
    mac_from_ =
        UserBoundNlMessage::StringFromMacAddress(&frame->destination_mac[0]);
    mac_to_ = UserBoundNlMessage::StringFromMacAddress(&frame->source_mac[0]);
    frame_type_ = frame->frame_control & kFrameTypeMask;

    switch (frame_type_) {
    case kAssocResponseFrameType:
    case kReassocResponseFrameType:
      status_ = le16toh(frame->u.associate_response.status_code);
      break;

    case kAuthFrameType:
      status_ = le16toh(frame->u.authentiate_message.status_code);
      break;

    case kDisassocFrameType:
    case kDeauthFrameType:
      status_ = le16toh(frame->u.deauthentiate_message.reason_code);
      break;

    default:
      break;
    }
  }
}

Nl80211Frame::~Nl80211Frame() {
  delete [] frame_;
  frame_ = NULL;
}

bool Nl80211Frame::ToString(string *output) {
  if (!output) {
    LOG(ERROR) << "NULL |output|";
    return false;
  }

  if ((byte_count_ == 0) || (frame_ == reinterpret_cast<uint8_t *>(NULL))) {
    output->append(" [no frame]");
    return true;
  }

  if (byte_count_ < kMinimumFrameByteCount) {
    output->append(" [invalid frame: ");
  } else {
    StringAppendF(output, " %s -> %s", mac_from_.c_str(), mac_to_.c_str());

    switch (frame_[0] & kFrameTypeMask) {
    case kAssocResponseFrameType:
      StringAppendF(output, "; AssocResponse status: %u: %s",
                    status_,
                    UserBoundNlMessage::StringFromStatus(status_).c_str());
      break;
    case kReassocResponseFrameType:
      StringAppendF(output, "; ReassocResponse status: %u: %s",
                    status_,
                    UserBoundNlMessage::StringFromStatus(status_).c_str());
      break;
    case kAuthFrameType:
      StringAppendF(output, "; Auth status: %u: %s",
                    status_,
                    UserBoundNlMessage::StringFromStatus(status_).c_str());
      break;

    case kDisassocFrameType:
      StringAppendF(output, "; Disassoc reason %u: %s",
                    status_,
                    UserBoundNlMessage::StringFromStatus(status_).c_str());
      break;
    case kDeauthFrameType:
      StringAppendF(output, "; Deauth reason %u: %s",
                    status_,
                    UserBoundNlMessage::StringFromStatus(status_).c_str());
      break;

    default:
      break;
    }
    output->append(" [frame: ");
  }

  for (int i = 0; i < byte_count_; ++i) {
    StringAppendF(output, "%02x, ", frame_[i]);
  }
  output->append("]");

  return true;
}

bool Nl80211Frame::IsEqual(const Nl80211Frame &other) {
  if (byte_count_ != other.byte_count_) {
    return false;
  }

  for (int i = 0; i < byte_count_; ++i) {
    if (frame_[i] != other.frame_[i]) {
      return false;
    }
  }

  return true;
}

//
// Specific UserBoundNlMessage types.
//

const uint8_t AssociateMessage::kCommand = NL80211_CMD_ASSOCIATE;
const char AssociateMessage::kCommandString[] = "NL80211_CMD_ASSOCIATE";

string AssociateMessage::ToString() const {
  string output(GetHeaderString());
  output.append("assoc");
  if (AttributeExists(NL80211_ATTR_FRAME))
    output.append(StringFromFrame(NL80211_ATTR_FRAME));
  else if (AttributeExists(NL80211_ATTR_TIMED_OUT))
    output.append(": timed out");
  else
    output.append(": unknown event");
  return output;
}

const uint8_t AuthenticateMessage::kCommand = NL80211_CMD_AUTHENTICATE;
const char AuthenticateMessage::kCommandString[] = "NL80211_CMD_AUTHENTICATE";

string AuthenticateMessage::ToString() const {
  string output(GetHeaderString());
  output.append("auth");
  if (AttributeExists(NL80211_ATTR_FRAME)) {
    output.append(StringFromFrame(NL80211_ATTR_FRAME));
  } else {
    output.append(AttributeExists(NL80211_ATTR_TIMED_OUT) ?
                  ": timed out" : ": unknown event");
  }
  return output;
}

const uint8_t CancelRemainOnChannelMessage::kCommand =
  NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL;
const char CancelRemainOnChannelMessage::kCommandString[] =
  "NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL";

string CancelRemainOnChannelMessage::ToString() const {
  string output(GetHeaderString());
  uint32_t freq;
  uint64_t cookie;
  StringAppendF(&output,
                "done with remain on freq %" PRIu32 " (cookie %" PRIx64 ")",
                (GetU32Attribute(NL80211_ATTR_WIPHY_FREQ, &freq) ? 0 : freq),
                (GetU64Attribute(NL80211_ATTR_COOKIE, &cookie) ? 0 : cookie));
  return output;
}

const uint8_t ConnectMessage::kCommand = NL80211_CMD_CONNECT;
const char ConnectMessage::kCommandString[] = "NL80211_CMD_CONNECT";

string ConnectMessage::ToString() const {
  string output(GetHeaderString());

  uint16_t status = UINT16_MAX;

  if (!GetU16Attribute(NL80211_ATTR_STATUS_CODE, &status)) {
    output.append("unknown connect status");
  } else if (status == 0) {
    output.append("connected");
  } else {
    output.append("failed to connect");
  }

  if (AttributeExists(NL80211_ATTR_MAC)) {
    string mac;
    GetMacAttributeString(NL80211_ATTR_MAC, &mac);
    StringAppendF(&output, " to %s", mac.c_str());
  }
  if (status)
    StringAppendF(&output, ", status: %u: %s", status,
                  StringFromStatus(status).c_str());
  return output;
}

const uint8_t DeauthenticateMessage::kCommand = NL80211_CMD_DEAUTHENTICATE;
const char DeauthenticateMessage::kCommandString[] =
    "NL80211_CMD_DEAUTHENTICATE";

string DeauthenticateMessage::ToString() const {
  string output(GetHeaderString());
  StringAppendF(&output, "deauth%s",
                StringFromFrame(NL80211_ATTR_FRAME).c_str());
  return output;
}

const uint8_t DeleteStationMessage::kCommand = NL80211_CMD_DEL_STATION;
const char DeleteStationMessage::kCommandString[] = "NL80211_CMD_DEL_STATION";

string DeleteStationMessage::ToString() const {
  string mac;
  GetMacAttributeString(NL80211_ATTR_MAC, &mac);
  string output(GetHeaderString());
  StringAppendF(&output, "del station %s", mac.c_str());
  return output;
}

const uint8_t DisassociateMessage::kCommand = NL80211_CMD_DISASSOCIATE;
const char DisassociateMessage::kCommandString[] = "NL80211_CMD_DISASSOCIATE";

string DisassociateMessage::ToString() const {
  string output(GetHeaderString());
  StringAppendF(&output, "disassoc%s",
                StringFromFrame(NL80211_ATTR_FRAME).c_str());
  return output;
}

const uint8_t DisconnectMessage::kCommand = NL80211_CMD_DISCONNECT;
const char DisconnectMessage::kCommandString[] = "NL80211_CMD_DISCONNECT";

string DisconnectMessage::ToString() const {
  string output(GetHeaderString());
  StringAppendF(&output, "disconnected %s",
                ((AttributeExists(NL80211_ATTR_DISCONNECTED_BY_AP)) ?
                  "(by AP)" : "(local request)"));

  uint16_t reason = UINT16_MAX;
  if (GetU16Attribute(NL80211_ATTR_REASON_CODE, &reason)) {
    StringAppendF(&output, " reason: %u: %s",
                  reason, StringFromStatus(reason).c_str());
  }
  return output;
}

const uint8_t FrameTxStatusMessage::kCommand = NL80211_CMD_FRAME_TX_STATUS;
const char FrameTxStatusMessage::kCommandString[] =
    "NL80211_CMD_FRAME_TX_STATUS";

string FrameTxStatusMessage::ToString() const {
  string output(GetHeaderString());
  uint64_t cookie = UINT64_MAX;
  GetU64Attribute(NL80211_ATTR_COOKIE, &cookie);

  StringAppendF(&output, "mgmt TX status (cookie %" PRIx64 "): %s",
                cookie,
                (AttributeExists(NL80211_ATTR_ACK) ? "acked" : "no ack"));
  return output;
}

const uint8_t JoinIbssMessage::kCommand = NL80211_CMD_JOIN_IBSS;
const char JoinIbssMessage::kCommandString[] = "NL80211_CMD_JOIN_IBSS";

string JoinIbssMessage::ToString() const {
  string mac;
  GetMacAttributeString(NL80211_ATTR_MAC, &mac);
  string output(GetHeaderString());
  StringAppendF(&output, "IBSS %s joined", mac.c_str());
  return output;
}

const uint8_t MichaelMicFailureMessage::kCommand =
    NL80211_CMD_MICHAEL_MIC_FAILURE;
const char MichaelMicFailureMessage::kCommandString[] =
    "NL80211_CMD_MICHAEL_MIC_FAILURE";

string MichaelMicFailureMessage::ToString() const {
  string output(GetHeaderString());

  output.append("Michael MIC failure event:");

  if (AttributeExists(NL80211_ATTR_MAC)) {
    string mac;
    GetMacAttributeString(NL80211_ATTR_MAC, &mac);
    StringAppendF(&output, " source MAC address %s", mac.c_str());
  }

  if (AttributeExists(NL80211_ATTR_KEY_SEQ)) {
    void *rawdata = NULL;
    int length = 0;
    if (GetRawAttributeData(NL80211_ATTR_KEY_SEQ, &rawdata, &length) &&
        rawdata && length == 6) {
      const unsigned char *seq = reinterpret_cast<const unsigned char *>(
          rawdata);
      StringAppendF(&output, " seq=%02x%02x%02x%02x%02x%02x",
                    seq[0], seq[1], seq[2], seq[3], seq[4], seq[5]);
    }
  }
  uint32_t key_type_val = UINT32_MAX;
  if (GetU32Attribute(NL80211_ATTR_KEY_TYPE, &key_type_val)) {
    enum nl80211_key_type key_type =
        static_cast<enum nl80211_key_type >(key_type_val);
    StringAppendF(&output, " Key Type %s", StringFromKeyType(key_type).c_str());
  }

  uint8_t key_index = UINT8_MAX;
  if (GetU8Attribute(NL80211_ATTR_KEY_IDX, &key_index)) {
    StringAppendF(&output, " Key Id %u", key_index);
  }

  return output;
}

const uint8_t NewScanResultsMessage::kCommand = NL80211_CMD_NEW_SCAN_RESULTS;
const char NewScanResultsMessage::kCommandString[] =
    "NL80211_CMD_NEW_SCAN_RESULTS";

string NewScanResultsMessage::ToString() const {
  string output(GetHeaderString());
  output.append("scan finished");

  {
    output.append("; frequencies: ");
    vector<uint32_t> list;
    if (GetScanFrequenciesAttribute(NL80211_ATTR_SCAN_FREQUENCIES, &list)) {
      string str;
      for (vector<uint32_t>::const_iterator i = list.begin();
             i != list.end(); ++i) {
        StringAppendF(&str, " %" PRIu32 ", ", *i);
      }
      output.append(str);
    }
  }

  {
    output.append("; SSIDs: ");
    vector<string> list;
    if (GetScanSsidsAttribute(NL80211_ATTR_SCAN_SSIDS, &list)) {
      string str;
      for (vector<string>::const_iterator i = list.begin();
           i != list.end(); ++i) {
        StringAppendF(&str, "\"%s\", ", i->c_str());
      }
      output.append(str);
    }
  }

  return output;
}

const uint8_t NewStationMessage::kCommand = NL80211_CMD_NEW_STATION;
const char NewStationMessage::kCommandString[] = "NL80211_CMD_NEW_STATION";

string NewStationMessage::ToString() const {
  string mac;
  GetMacAttributeString(NL80211_ATTR_MAC, &mac);
  string output(GetHeaderString());
  StringAppendF(&output, "new station %s", mac.c_str());

  return output;
}

const uint8_t NewWifiMessage::kCommand = NL80211_CMD_NEW_WIPHY;
const char NewWifiMessage::kCommandString[] = "NL80211_CMD_NEW_WIPHY";

string NewWifiMessage::ToString() const {
  string output(GetHeaderString());
  string wifi_name = "None";
  GetStringAttribute(NL80211_ATTR_WIPHY_NAME, &wifi_name);
  StringAppendF(&output, "renamed to %s", wifi_name.c_str());
  return output;
}

const uint8_t NotifyCqmMessage::kCommand = NL80211_CMD_NOTIFY_CQM;
const char NotifyCqmMessage::kCommandString[] = "NL80211_CMD_NOTIFY_CQM";

string NotifyCqmMessage::ToString() const {
  static const nla_policy kCqmPolicy[NL80211_ATTR_CQM_MAX + 1] = {
    { NLA_U32, 0, 0 },  // Who Knows?
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_THOLD]
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_HYST]
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]
  };

  string output(GetHeaderString());
  output.append("connection quality monitor event: ");

  const nlattr *const_data = GetAttribute(NL80211_ATTR_CQM);
  nlattr *cqm_attr = const_cast<nlattr *>(const_data);

  nlattr *cqm[NL80211_ATTR_CQM_MAX + 1];
  if (!AttributeExists(NL80211_ATTR_CQM) || !cqm_attr ||
      nla_parse_nested(cqm, NL80211_ATTR_CQM_MAX, cqm_attr,
                       const_cast<nla_policy *>(kCqmPolicy))) {
    output.append("missing data!");
    return output;
  }
  if (cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]) {
    enum nl80211_cqm_rssi_threshold_event rssi_event =
        static_cast<enum nl80211_cqm_rssi_threshold_event>(
          nla_get_u32(cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]));
    if (rssi_event == NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH)
      output.append("RSSI went above threshold");
    else
      output.append("RSSI went below threshold");
  } else if (cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT] &&
       AttributeExists(NL80211_ATTR_MAC)) {
    string mac;
    GetMacAttributeString(NL80211_ATTR_MAC, &mac);
    StringAppendF(&output, "peer %s didn't ACK %" PRIu32 " packets",
                  mac.c_str(),
                  nla_get_u32(cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT]));
  } else {
    output.append("unknown event");
  }

  return output;
}

const uint8_t PmksaCandidateMessage::kCommand = NL80211_ATTR_PMKSA_CANDIDATE;
const char PmksaCandidateMessage::kCommandString[] =
  "NL80211_ATTR_PMKSA_CANDIDATE";

string PmksaCandidateMessage::ToString() const {
  string output(GetHeaderString());
  output.append("PMKSA candidate found");
  return output;
}

const uint8_t RegBeaconHintMessage::kCommand = NL80211_CMD_REG_BEACON_HINT;
const char RegBeaconHintMessage::kCommandString[] =
    "NL80211_CMD_REG_BEACON_HINT";

string RegBeaconHintMessage::ToString() const {
  string output(GetHeaderString());
  uint32_t wiphy_idx = UINT32_MAX;
  GetU32Attribute(NL80211_ATTR_WIPHY, &wiphy_idx);

  const nlattr *const_before = GetAttribute(NL80211_ATTR_FREQ_BEFORE);
  ieee80211_beacon_channel chan_before_beacon;
  memset(&chan_before_beacon, 0, sizeof(chan_before_beacon));
  if (ParseBeaconHintChan(const_before, &chan_before_beacon))
    return "";

  const nlattr *const_after = GetAttribute(NL80211_ATTR_FREQ_AFTER);
  ieee80211_beacon_channel chan_after_beacon;
  memset(&chan_after_beacon, 0, sizeof(chan_after_beacon));
  if (ParseBeaconHintChan(const_after, &chan_after_beacon))
    return "";

  if (chan_before_beacon.center_freq != chan_after_beacon.center_freq)
    return "";

  /* A beacon hint is sent _only_ if something _did_ change */
  output.append("beacon hint:");
  StringAppendF(&output, "phy%" PRIu32 " %u MHz [%d]:",
                wiphy_idx, chan_before_beacon.center_freq,
                ChannelFromIeee80211Frequency(chan_before_beacon.center_freq));

  if (chan_before_beacon.passive_scan && !chan_after_beacon.passive_scan)
    output.append("\to active scanning enabled");
  if (chan_before_beacon.no_ibss && !chan_after_beacon.no_ibss)
    output.append("\to beaconing enabled");
  return output;
}

int RegBeaconHintMessage::ParseBeaconHintChan(const nlattr *tb,
                                              ieee80211_beacon_channel *chan)
    const {
  static const nla_policy kBeaconFreqPolicy[
      NL80211_FREQUENCY_ATTR_MAX + 1] = {
        {0, 0, 0},
        { NLA_U32, 0, 0 },  // [NL80211_FREQUENCY_ATTR_FREQ]
        {0, 0, 0},
        { NLA_FLAG, 0, 0 },  // [NL80211_FREQUENCY_ATTR_PASSIVE_SCAN]
        { NLA_FLAG, 0, 0 },  // [NL80211_FREQUENCY_ATTR_NO_IBSS]
  };

  if (!tb) {
    LOG(ERROR) << "|tb| parameter is NULL.";
    return -EINVAL;
  }

  if (!chan) {
    LOG(ERROR) << "|chan| parameter is NULL.";
    return -EINVAL;
  }

  nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];

  if (nla_parse_nested(tb_freq,
                       NL80211_FREQUENCY_ATTR_MAX,
                       const_cast<nlattr *>(tb),
                       const_cast<nla_policy *>(kBeaconFreqPolicy)))
    return -EINVAL;

  chan->center_freq = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);

  if (tb_freq[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN])
    chan->passive_scan = true;
  if (tb_freq[NL80211_FREQUENCY_ATTR_NO_IBSS])
    chan->no_ibss = true;

  return 0;
}

int RegBeaconHintMessage::ChannelFromIeee80211Frequency(int freq) {
  // TODO(wdg): get rid of these magic numbers.
  if (freq == 2484)
    return 14;

  if (freq < 2484)
    return (freq - 2407) / 5;

  /* FIXME: dot11ChannelStartingFactor (802.11-2007 17.3.8.3.2) */
  return freq/5 - 1000;
}

const uint8_t RegChangeMessage::kCommand = NL80211_CMD_REG_CHANGE;
const char RegChangeMessage::kCommandString[] = "NL80211_CMD_REG_CHANGE";

string RegChangeMessage::ToString() const {
  string output(GetHeaderString());
  output.append("regulatory domain change: ");

  uint8_t reg_type = UINT8_MAX;
  GetU8Attribute(NL80211_ATTR_REG_TYPE, &reg_type);

  uint32_t initiator = UINT32_MAX;
  GetU32Attribute(NL80211_ATTR_REG_INITIATOR, &initiator);

  uint32_t wifi = UINT32_MAX;
  bool wifi_exists = GetU32Attribute(NL80211_ATTR_WIPHY, &wifi);

  string alpha2 = "<None>";
  GetStringAttribute(NL80211_ATTR_REG_ALPHA2, &alpha2);

  switch (reg_type) {
  case NL80211_REGDOM_TYPE_COUNTRY:
    StringAppendF(&output, "set to %s by %s request",
                  alpha2.c_str(), StringFromRegInitiator(initiator).c_str());
    if (wifi_exists)
      StringAppendF(&output, " on phy%" PRIu32, wifi);
    break;

  case NL80211_REGDOM_TYPE_WORLD:
    StringAppendF(&output, "set to world roaming by %s request",
                  StringFromRegInitiator(initiator).c_str());
    break;

  case NL80211_REGDOM_TYPE_CUSTOM_WORLD:
    StringAppendF(&output,
                  "custom world roaming rules in place on phy%" PRIu32
                  " by %s request",
                  wifi, StringFromRegInitiator(initiator).c_str());
    break;

  case NL80211_REGDOM_TYPE_INTERSECTION:
    StringAppendF(&output, "intersection used due to a request made by %s",
                  StringFromRegInitiator(initiator).c_str());
    if (wifi_exists)
      StringAppendF(&output, " on phy%" PRIu32, wifi);
    break;

  default:
    output.append("unknown source");
    break;
  }
  return output;
}

const uint8_t RemainOnChannelMessage::kCommand = NL80211_CMD_REMAIN_ON_CHANNEL;
const char RemainOnChannelMessage::kCommandString[] =
    "NL80211_CMD_REMAIN_ON_CHANNEL";

string RemainOnChannelMessage::ToString() const {
  string output(GetHeaderString());

  uint32_t wifi_freq = UINT32_MAX;
  GetU32Attribute(NL80211_ATTR_WIPHY_FREQ, &wifi_freq);

  uint32_t duration = UINT32_MAX;
  GetU32Attribute(NL80211_ATTR_DURATION, &duration);

  uint64_t cookie = UINT64_MAX;
  GetU64Attribute(NL80211_ATTR_COOKIE, &cookie);

  StringAppendF(&output, "remain on freq %" PRIu32 " (%" PRIu32 "ms, cookie %"
                PRIx64 ")",
                wifi_freq, duration, cookie);
  return output;
}

const uint8_t RoamMessage::kCommand = NL80211_CMD_ROAM;
const char RoamMessage::kCommandString[] = "NL80211_CMD_ROAM";

string RoamMessage::ToString() const {
  string output(GetHeaderString());
  output.append("roamed");

  if (AttributeExists(NL80211_ATTR_MAC)) {
    string mac;
    GetMacAttributeString(NL80211_ATTR_MAC, &mac);
    StringAppendF(&output, " to %s", mac.c_str());
  }
  return output;
}

const uint8_t ScanAbortedMessage::kCommand = NL80211_CMD_SCAN_ABORTED;
const char ScanAbortedMessage::kCommandString[] = "NL80211_CMD_SCAN_ABORTED";

string ScanAbortedMessage::ToString() const {
  string output(GetHeaderString());
  output.append("scan aborted");

  {
    output.append("; frequencies: ");
    vector<uint32_t> list;
    if (GetScanFrequenciesAttribute(NL80211_ATTR_SCAN_FREQUENCIES, &list)) {
      string str;
      for (vector<uint32_t>::const_iterator i = list.begin();
           i != list.end(); ++i) {
        StringAppendF(&str, " %" PRIu32 ", ", *i);
      }
      output.append(str);
    }
  }

  {
    output.append("; SSIDs: ");
    vector<string> list;
    if (GetScanSsidsAttribute(NL80211_ATTR_SCAN_SSIDS, &list)) {
      string str;
      for (vector<string>::const_iterator i = list.begin();
           i != list.end(); ++i) {
        StringAppendF(&str, "\"%s\", ", i->c_str());
      }
      output.append(str);
    }
  }

  return output;
}

const uint8_t TriggerScanMessage::kCommand = NL80211_CMD_TRIGGER_SCAN;
const char TriggerScanMessage::kCommandString[] = "NL80211_CMD_TRIGGER_SCAN";

string TriggerScanMessage::ToString() const {
  string output(GetHeaderString());
  output.append("scan started");

  {
    output.append("; frequencies: ");
    vector<uint32_t> list;
    if (GetScanFrequenciesAttribute(NL80211_ATTR_SCAN_FREQUENCIES, &list)) {
      string str;
      for (vector<uint32_t>::const_iterator i = list.begin();
             i != list.end(); ++i) {
        StringAppendF(&str, "%" PRIu32 ", ", *i);
      }
      output.append(str);
    }
  }

  {
    output.append("; SSIDs: ");
    vector<string> list;
    if (GetScanSsidsAttribute(NL80211_ATTR_SCAN_SSIDS, &list)) {
      string str;
      for (vector<string>::const_iterator i = list.begin();
           i != list.end(); ++i) {
        StringAppendF(&str, "\"%s\", ", i->c_str());
      }
      output.append(str);
    }
  }

  return output;
}

const uint8_t UnknownMessage::kCommand = 0xff;
const char UnknownMessage::kCommandString[] = "<Unknown Message Type>";

string UnknownMessage::ToString() const {
  string output(GetHeaderString());
  StringAppendF(&output, "unknown event %u", command_);
  return output;
}

const uint8_t UnprotDeauthenticateMessage::kCommand =
    NL80211_CMD_UNPROT_DEAUTHENTICATE;
const char UnprotDeauthenticateMessage::kCommandString[] =
    "NL80211_CMD_UNPROT_DEAUTHENTICATE";

string UnprotDeauthenticateMessage::ToString() const {
  string output(GetHeaderString());
  StringAppendF(&output, "unprotected deauth %s",
                StringFromFrame(NL80211_ATTR_FRAME).c_str());
  return output;
}

const uint8_t UnprotDisassociateMessage::kCommand =
    NL80211_CMD_UNPROT_DISASSOCIATE;
const char UnprotDisassociateMessage::kCommandString[] =
    "NL80211_CMD_UNPROT_DISASSOCIATE";

string UnprotDisassociateMessage::ToString() const {
  string output(GetHeaderString());
  StringAppendF(&output, "unprotected disassoc %s",
                StringFromFrame(NL80211_ATTR_FRAME).c_str());
  return output;
}

//
// Factory class.
//

UserBoundNlMessage *UserBoundNlMessageFactory::CreateMessage(nlmsghdr *msg) {
  if (!msg) {
    LOG(ERROR) << "NULL |msg| parameter";
    return NULL;
  }

  scoped_ptr<UserBoundNlMessage> message;
  genlmsghdr *gnlh =
      reinterpret_cast<genlmsghdr *>(nlmsg_data(msg));

  if (!gnlh) {
    LOG(ERROR) << "NULL gnlh";
    return NULL;
  }

  switch (gnlh->cmd) {
    case AssociateMessage::kCommand:
      message.reset(new AssociateMessage()); break;
    case AuthenticateMessage::kCommand:
      message.reset(new AuthenticateMessage()); break;
    case CancelRemainOnChannelMessage::kCommand:
      message.reset(new CancelRemainOnChannelMessage()); break;
    case ConnectMessage::kCommand:
      message.reset(new ConnectMessage()); break;
    case DeauthenticateMessage::kCommand:
      message.reset(new DeauthenticateMessage()); break;

#if 0
    // TODO(wdg): our version of 'iw' doesn't have this so I can't put it in
    // without breaking the diff.  Remove the 'if 0' after the unit tests are
    // added.
    case DeleteStationMessage::kCommand:
      message.reset(new DeleteStationMessage()); break;
#endif

    case DisassociateMessage::kCommand:
      message.reset(new DisassociateMessage()); break;
    case DisconnectMessage::kCommand:
      message.reset(new DisconnectMessage()); break;
    case FrameTxStatusMessage::kCommand:
      message.reset(new FrameTxStatusMessage()); break;
    case JoinIbssMessage::kCommand:
      message.reset(new JoinIbssMessage()); break;
    case MichaelMicFailureMessage::kCommand:
      message.reset(new MichaelMicFailureMessage()); break;
    case NewScanResultsMessage::kCommand:
      message.reset(new NewScanResultsMessage()); break;
    case NewStationMessage::kCommand:
      message.reset(new NewStationMessage()); break;
    case NewWifiMessage::kCommand:
      message.reset(new NewWifiMessage()); break;
    case NotifyCqmMessage::kCommand:
      message.reset(new NotifyCqmMessage()); break;
    case PmksaCandidateMessage::kCommand:
      message.reset(new PmksaCandidateMessage()); break;
    case RegBeaconHintMessage::kCommand:
      message.reset(new RegBeaconHintMessage()); break;
    case RegChangeMessage::kCommand:
      message.reset(new RegChangeMessage()); break;
    case RemainOnChannelMessage::kCommand:
      message.reset(new RemainOnChannelMessage()); break;
    case RoamMessage::kCommand:
      message.reset(new RoamMessage()); break;
    case ScanAbortedMessage::kCommand:
      message.reset(new ScanAbortedMessage()); break;
    case TriggerScanMessage::kCommand:
      message.reset(new TriggerScanMessage()); break;
    case UnprotDeauthenticateMessage::kCommand:
      message.reset(new UnprotDeauthenticateMessage()); break;
    case UnprotDisassociateMessage::kCommand:
      message.reset(new UnprotDisassociateMessage()); break;

    default:
      message.reset(new UnknownMessage(gnlh->cmd)); break;
      break;
  }

  nlattr *tb[NL80211_ATTR_MAX + 1];

  // Parse the attributes from the nl message payload (which starts at the
  // header) into the 'tb' array.
  nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
    genlmsg_attrlen(gnlh, 0), NULL);

  if (!message->Init(tb, msg)) {
    LOG(ERROR) << "Message did not initialize properly";
    return NULL;
  }

//#if 0  // If we're collecting data for unit tests, uncommnet this.
  UserBoundNlMessageDataCollector::GetInstance()->CollectDebugData(*message,
                                                                   msg);
//#endif // 0

  return message.release();
}

UserBoundNlMessageDataCollector *
    UserBoundNlMessageDataCollector::GetInstance() {
  return g_datacollector.Pointer();
}

UserBoundNlMessageDataCollector::UserBoundNlMessageDataCollector() {
  need_to_print[NL80211_ATTR_PMKSA_CANDIDATE] = true;
  need_to_print[NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL] = true;
  need_to_print[NL80211_CMD_DEL_STATION] = true;
  need_to_print[NL80211_CMD_FRAME_TX_STATUS] = true;
  need_to_print[NL80211_CMD_JOIN_IBSS] = true;
  need_to_print[NL80211_CMD_MICHAEL_MIC_FAILURE] = true;
  need_to_print[NL80211_CMD_NEW_WIPHY] = true;
  need_to_print[NL80211_CMD_REG_BEACON_HINT] = true;
  need_to_print[NL80211_CMD_REG_CHANGE] = true;
  need_to_print[NL80211_CMD_REMAIN_ON_CHANNEL] = true;
  need_to_print[NL80211_CMD_ROAM] = true;
  need_to_print[NL80211_CMD_SCAN_ABORTED] = true;
  need_to_print[NL80211_CMD_UNPROT_DEAUTHENTICATE] = true;
  need_to_print[NL80211_CMD_UNPROT_DISASSOCIATE] = true;
}

void UserBoundNlMessageDataCollector::CollectDebugData(
    const UserBoundNlMessage &message, nlmsghdr *msg)
{
  if (!msg) {
    LOG(ERROR) << "NULL |msg| parameter";
    return;
  }

  bool doit = false;

  map<uint8_t, bool>::const_iterator node;
  node = need_to_print.find(message.GetMessageType());
  if (node != need_to_print.end())
    doit = node->second;

  if (doit) {
    LOG(ERROR) << "@@const unsigned char "
               << "k" << message.GetMessageTypeString()
               << "[] = { ";

    int payload_bytes = nlmsg_len(msg);

    size_t bytes = nlmsg_total_size(payload_bytes);
    unsigned char *rawdata = reinterpret_cast<unsigned char *>(msg);
    for (size_t i=0; i<bytes; ++i) {
      LOG(ERROR) << "  0x"
                 << std::hex << std::setfill('0') << std::setw(2)
                 << + rawdata[i] << ",";
    }
    LOG(ERROR) << "};";
    need_to_print[message.GetMessageType()] = false;
  }
}

}  // namespace shill.
