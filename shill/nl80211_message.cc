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

#include "shill/nl80211_message.h"

#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netlink/attr.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>

#include <algorithm>
#include <iomanip>
#include <string>

#include <base/bind.h>
#include <base/format_macros.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>

#include "shill/attribute_list.h"
#include "shill/ieee80211.h"
#include "shill/logging.h"
#include "shill/netlink_socket.h"
#include "shill/nl80211_attribute.h"
#include "shill/scope_logger.h"
#include "shill/wifi.h"

using base::Bind;
using base::LazyInstance;
using base::StringAppendF;
using base::StringPrintf;
using std::map;
using std::min;
using std::string;
using std::vector;

namespace shill {

namespace {
LazyInstance<Nl80211MessageDataCollector> g_datacollector =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

const char Nl80211Message::kBogusMacAddress[]="XX:XX:XX:XX:XX:XX";

const uint8_t Nl80211Frame::kMinimumFrameByteCount = 26;
const uint8_t Nl80211Frame::kFrameTypeMask = 0xfc;

const uint32_t Nl80211Message::kIllegalMessage = 0;
const unsigned int Nl80211Message::kEthernetAddressBytes = 6;
map<uint16_t, string> *Nl80211Message::reason_code_string_ = NULL;
map<uint16_t, string> *Nl80211Message::status_code_string_ = NULL;

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
// Nl80211Message
//

void Nl80211Message::Print(int log_level) const {
  SLOG(WiFi, log_level) << StringPrintf("Message %s (%d)",
                                        message_type_string(),
                                        message_type());
  attributes_.Print(log_level, 1);
}

bool Nl80211Message::InitFromNlmsg(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "Null |msg| parameter";
    return false;
  }

  // Netlink header.
  sequence_number_ = const_msg->nlmsg_seq;
  SLOG(WiFi, 6) << "NL Message " << sequence_number() << " <===";

  // Casting away constness, here, since the libnl code doesn't properly label
  // their stuff as const (even though it is).
  nlmsghdr *msg = const_cast<nlmsghdr *>(const_msg);

  // Genl message header.
  genlmsghdr *gnlh = reinterpret_cast<genlmsghdr *>(nlmsg_data(msg));

  // Attributes.
  // Parse the attributes from the nl message payload into the 'tb' array.
  nlattr *tb[NL80211_ATTR_MAX + 1];
  nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

  for (int i = 0; i < NL80211_ATTR_MAX + 1; ++i) {
    if (tb[i]) {
      // TODO(wdg): When Nl80211Messages instantiate their own attributes,
      // this call should, instead, call |SetAttributeFromNlAttr|.
      attributes_.CreateAndInitAttribute(
          i, tb[i], Bind(&NetlinkAttribute::NewNl80211AttributeFromId));
    }
  }

  // Convert integer values provided by libnl (for example, from the
  // NL80211_ATTR_STATUS_CODE or NL80211_ATTR_REASON_CODE attribute) into
  // strings describing the status.
  if (!reason_code_string_) {
    reason_code_string_ = new map<uint16_t, string>;
    (*reason_code_string_)[IEEE_80211::kReasonCodeUnspecified] =
        "Unspecified reason";
    (*reason_code_string_)[
        IEEE_80211::kReasonCodePreviousAuthenticationInvalid] =
        "Previous authentication no longer valid";
    (*reason_code_string_)[IEEE_80211::kReasonCodeSenderHasLeft] =
        "Deauthentcated because sending STA is leaving (or has left) IBSS or "
        "ESS";
    (*reason_code_string_)[IEEE_80211::kReasonCodeInactivity] =
        "Disassociated due to inactivity";
    (*reason_code_string_)[IEEE_80211::kReasonCodeTooManySTAs] =
        "Disassociated because AP is unable to handle all currently associated "
        "STAs";
    (*reason_code_string_)[IEEE_80211::kReasonCodeNonAuthenticated] =
        "Class 2 frame received from nonauthenticated STA";
    (*reason_code_string_)[IEEE_80211::kReasonCodeNonAssociated] =
        "Class 3 frame received from nonassociated STA";
    (*reason_code_string_)[IEEE_80211::kReasonCodeDisassociatedHasLeft] =
        "Disassociated because sending STA is leaving (or has left) BSS";
    (*reason_code_string_)[
        IEEE_80211::kReasonCodeReassociationNotAuthenticated] =
        "STA requesting (re)association is not authenticated with responding "
        "STA";
    (*reason_code_string_)[IEEE_80211::kReasonCodeUnacceptablePowerCapability] =
        "Disassociated because the information in the Power Capability "
        "element is unacceptable";
    (*reason_code_string_)[
        IEEE_80211::kReasonCodeUnacceptableSupportedChannelInfo] =
        "Disassociated because the information in the Supported Channels "
        "element is unacceptable";
    (*reason_code_string_)[IEEE_80211::kReasonCodeInvalidInfoElement] =
        "Invalid information element, i.e., an information element defined in "
        "this standard for which the content does not meet the specifications "
        "in Clause 7";
    (*reason_code_string_)[IEEE_80211::kReasonCodeMICFailure] =
        "Message integrity code (MIC) failure";
    (*reason_code_string_)[IEEE_80211::kReasonCode4WayTimeout] =
        "4-Way Handshake timeout";
    (*reason_code_string_)[IEEE_80211::kReasonCodeGroupKeyHandshakeTimeout] =
        "Group Key Handshake timeout";
    (*reason_code_string_)[IEEE_80211::kReasonCodeDifferenIE] =
        "Information element in 4-Way Handshake different from "
        "(Re)Association Request/Probe Response/Beacon frame";
    (*reason_code_string_)[IEEE_80211::kReasonCodeGroupCipherInvalid] =
        "Invalid group cipher";
    (*reason_code_string_)[IEEE_80211::kReasonCodePairwiseCipherInvalid] =
        "Invalid pairwise cipher";
    (*reason_code_string_)[IEEE_80211::kReasonCodeAkmpInvalid] =
        "Invalid AKMP";
    (*reason_code_string_)[IEEE_80211::kReasonCodeUnsupportedRsnIeVersion] =
        "Unsupported RSN information element version";
    (*reason_code_string_)[IEEE_80211::kReasonCodeInvalidRsnIeCaps] =
        "Invalid RSN information element capabilities";
    (*reason_code_string_)[IEEE_80211::kReasonCode8021XAuth] =
        "IEEE 802.1X authentication failed";
    (*reason_code_string_)[IEEE_80211::kReasonCodeCipherSuiteRejected] =
        "Cipher suite rejected because of the security policy";
    (*reason_code_string_)[IEEE_80211::kReasonCodeUnspecifiedQoS] =
        "Disassociated for unspecified, QoS-related reason";
    (*reason_code_string_)[IEEE_80211::kReasonCodeQoSBandwidth] =
        "Disassociated because QoS AP lacks sufficient bandwidth for this "
        "QoS STA";
    (*reason_code_string_)[IEEE_80211::kReasonCodeiPoorConditions] =
        "Disassociated because excessive number of frames need to be "
        "acknowledged, but are not acknowledged due to AP transmissions "
        "and/or poor channel conditions";
    (*reason_code_string_)[IEEE_80211::kReasonCodeOutsideTxop] =
        "Disassociated because STA is transmitting outside the limits of its "
        "TXOPs";
    (*reason_code_string_)[IEEE_80211::kReasonCodeStaLeaving] =
        "Requested from peer STA as the STA is leaving the BSS (or resetting)";
    (*reason_code_string_)[IEEE_80211::kReasonCodeUnacceptableMechanism] =
        "Requested from peer STA as it does not want to use the mechanism";
    (*reason_code_string_)[IEEE_80211::kReasonCodeSetupRequired] =
        "Requested from peer STA as the STA received frames using the "
        "mechanism for which a setup is required";
    (*reason_code_string_)[IEEE_80211::kReasonCodeTimeout] =
        "Requested from peer STA due to timeout";
    (*reason_code_string_)[IEEE_80211::kReasonCodeCipherSuiteNotSupported] =
        "Peer STA does not support the requested cipher suite";
    (*reason_code_string_)[IEEE_80211::kReasonCodeInvalid] = "<INVALID REASON>";
  }

  if (!status_code_string_) {
    status_code_string_ = new map<uint16_t, string>;
    (*status_code_string_)[IEEE_80211::kStatusCodeSuccessful] = "Successful";
    (*status_code_string_)[IEEE_80211::kStatusCodeFailure] =
        "Unspecified failure";
    (*status_code_string_)[IEEE_80211::kStatusCodeAllCapabilitiesNotSupported] =
        "Cannot support all requested capabilities in the capability "
        "information field";
    (*status_code_string_)[IEEE_80211::kStatusCodeCantConfirmAssociation] =
        "Reassociation denied due to inability to confirm that association "
        "exists";
    (*status_code_string_)[IEEE_80211::kStatusCodeAssociationDenied] =
        "Association denied due to reason outside the scope of this standard";
    (*status_code_string_)[
        IEEE_80211::kStatusCodeAuthenticationUnsupported] =
        "Responding station does not support the specified authentication "
        "algorithm";
    (*status_code_string_)[IEEE_80211::kStatusCodeOutOfSequence] =
        "Received an authentication frame with authentication transaction "
        "sequence number out of expected sequence";
    (*status_code_string_)[IEEE_80211::kStatusCodeChallengeFailure] =
        "Authentication rejected because of challenge failure";
    (*status_code_string_)[IEEE_80211::kStatusCodeFrameTimeout] =
        "Authentication rejected due to timeout waiting for next frame in "
        "sequence";
    (*status_code_string_)[IEEE_80211::kStatusCodeMaxSta] =
        "Association denied because AP is unable to handle additional "
        "associated STA";
    (*status_code_string_)[IEEE_80211::kStatusCodeDataRateUnsupported] =
        "Association denied due to requesting station not supporting all of "
        "the data rates in the BSSBasicRateSet parameter";
    (*status_code_string_)[IEEE_80211::kStatusCodeShortPreambleUnsupported] =
        "Association denied due to requesting station not supporting the "
        "short preamble option";
    (*status_code_string_)[IEEE_80211::kStatusCodePbccUnsupported] =
        "Association denied due to requesting station not supporting the PBCC "
        "modulation option";
    (*status_code_string_)[
        IEEE_80211::kStatusCodeChannelAgilityUnsupported] =
        "Association denied due to requesting station not supporting the "
        "channel agility option";
    (*status_code_string_)[IEEE_80211::kStatusCodeNeedSpectrumManagement] =
        "Association request rejected because Spectrum Management capability "
        "is required";
    (*status_code_string_)[
        IEEE_80211::kStatusCodeUnacceptablePowerCapability] =
        "Association request rejected because the information in the Power "
        "Capability element is unacceptable";
    (*status_code_string_)[
        IEEE_80211::kStatusCodeUnacceptableSupportedChannelInfo] =
        "Association request rejected because the information in the "
        "Supported Channels element is unacceptable";
    (*status_code_string_)[IEEE_80211::kStatusCodeShortTimeSlotRequired] =
        "Association request rejected due to requesting station not "
        "supporting the Short Slot Time option";
    (*status_code_string_)[IEEE_80211::kStatusCodeDssOfdmRequired] =
        "Association request rejected due to requesting station not "
        "supporting the DSSS-OFDM option";
    (*status_code_string_)[IEEE_80211::kStatusCodeQosFailure] =
        "Unspecified, QoS related failure";
    (*status_code_string_)[
        IEEE_80211::kStatusCodeInsufficientBandwithForQsta] =
        "Association denied due to QAP having insufficient bandwidth to handle "
        "another QSTA";
    (*status_code_string_)[IEEE_80211::kStatusCodePoorConditions] =
        "Association denied due to poor channel conditions";
    (*status_code_string_)[IEEE_80211::kStatusCodeQosNotSupported] =
        "Association (with QoS BSS) denied due to requesting station not "
        "supporting the QoS facility";
    (*status_code_string_)[IEEE_80211::kStatusCodeDeclined] =
        "The request has been declined";
    (*status_code_string_)[IEEE_80211::kStatusCodeInvalidParameterValues] =
        "The request has not been successful as one or more parameters have "
        "invalid values";
    (*status_code_string_)[IEEE_80211::kStatusCodeCannotBeHonored] =
        "The TS has not been created because the request cannot be honored. "
        "However, a suggested Tspec is provided so that the initiating QSTA "
        "may attempt to send another TS with the suggested changes to the "
        "TSpec";
    (*status_code_string_)[IEEE_80211::kStatusCodeInvalidInfoElement] =
        "Invalid Information Element";
    (*status_code_string_)[IEEE_80211::kStatusCodeGroupCipherInvalid] =
        "Invalid Group Cipher";
    (*status_code_string_)[IEEE_80211::kStatusCodePairwiseCipherInvalid] =
        "Invalid Pairwise Cipher";
    (*status_code_string_)[IEEE_80211::kStatusCodeAkmpInvalid] = "Invalid AKMP";
    (*status_code_string_)[IEEE_80211::kStatusCodeUnsupportedRsnIeVersion] =
        "Unsupported RSN Information Element version";
    (*status_code_string_)[IEEE_80211::kStatusCodeInvalidRsnIeCaps] =
        "Invalid RSN Information Element Capabilities";
    (*status_code_string_)[IEEE_80211::kStatusCodeCipherSuiteRejected] =
        "Cipher suite is rejected per security policy";
    (*status_code_string_)[IEEE_80211::kStatusCodeTsDelayNotMet] =
        "The TS has not been created. However, the HC may be capable of "
        "creating a TS, in response to a request, after the time indicated in "
        "the TS Delay element";
    (*status_code_string_)[IEEE_80211::kStatusCodeDirectLinkIllegal] =
        "Direct link is not allowed in the BSS by policy";
    (*status_code_string_)[IEEE_80211::kStatusCodeStaNotInBss] =
        "Destination STA is not present within this BSS";
    (*status_code_string_)[IEEE_80211::kStatusCodeStaNotInQsta] =
        "The destination STA is not a QoS STA";
    (*status_code_string_)[IEEE_80211::kStatusCodeExcessiveListenInterval] =
        "Association denied because Listen Interval is too large";
    (*status_code_string_)[IEEE_80211::kStatusCodeInvalid] = "<INVALID STATUS>";
  }

  return true;
}

// static
void Nl80211Message::PrintBytes(int log_level, const unsigned char *buf,
                                size_t num_bytes) {
  SLOG(WiFi, log_level) << "Netlink Message -- Examining Bytes";
  if (!buf) {
    SLOG(WiFi, log_level) << "<NULL Buffer>";
    return;
  }

  if (num_bytes >= sizeof(nlmsghdr)) {
    const nlmsghdr *header = reinterpret_cast<const nlmsghdr *>(buf);
    SLOG(WiFi, log_level) << StringPrintf(
        "len:          %02x %02x %02x %02x = %u bytes",
        buf[0], buf[1], buf[2], buf[3], header->nlmsg_len);

    SLOG(WiFi, log_level) << StringPrintf(
        "type | flags: %02x %02x %02x %02x - type:%u flags:%s%s%s%s%s",
        buf[4], buf[5], buf[6], buf[7], header->nlmsg_type,
        ((header->nlmsg_flags & NLM_F_REQUEST) ? " REQUEST" : ""),
        ((header->nlmsg_flags & NLM_F_MULTI) ? " MULTI" : ""),
        ((header->nlmsg_flags & NLM_F_ACK) ? " ACK" : ""),
        ((header->nlmsg_flags & NLM_F_ECHO) ? " ECHO" : ""),
        ((header->nlmsg_flags & NLM_F_DUMP_INTR) ? " BAD-SEQ" : ""));

    SLOG(WiFi, log_level) << StringPrintf(
        "sequence:     %02x %02x %02x %02x = %u",
        buf[8], buf[9], buf[10], buf[11], header->nlmsg_seq);
    SLOG(WiFi, log_level) << StringPrintf(
        "pid:          %02x %02x %02x %02x = %u",
        buf[12], buf[13], buf[14], buf[15], header->nlmsg_pid);
    buf += sizeof(nlmsghdr);
    num_bytes -= sizeof(nlmsghdr);
  } else {
    SLOG(WiFi, log_level) << "Not enough bytes (" << num_bytes
                          << ") for a complete nlmsghdr (requires "
                          << sizeof(nlmsghdr) << ").";
  }

  while (num_bytes) {
    string output;
    size_t bytes_this_row = min(num_bytes, static_cast<size_t>(32));
    for (size_t i = 0; i < bytes_this_row; ++i) {
      StringAppendF(&output, " %02x", *buf++);
    }
    SLOG(WiFi, log_level) << output;
    num_bytes -= bytes_this_row;
  }
}

// Helper function to provide a string for a MAC address.
bool Nl80211Message::GetMacAttributeString(int id, string *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  ByteString data;
  if (!attributes().GetRawAttributeValue(id, &data)) {
    value->assign(kBogusMacAddress);
    return false;
  }
  value->assign(StringFromMacAddress(data.GetConstData()));

  return true;
}

// Helper function to provide a string for NL80211_ATTR_SCAN_FREQUENCIES.
bool Nl80211Message::GetScanFrequenciesAttribute(
    int id, vector<uint32_t> *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  value->clear();
  ByteString rawdata;
  if (!attributes().GetRawAttributeValue(id, &rawdata) && !rawdata.IsEmpty())
    return false;

  nlattr *nst = NULL;
  // |nla_for_each_attr| requires a non-const parameter even though it
  // doesn't change the data.
  nlattr *attr_data = reinterpret_cast<nlattr *>(rawdata.GetData());
  int rem_nst;
  int len = rawdata.GetLength();

  nla_for_each_attr(nst, attr_data, len, rem_nst) {
    value->push_back(NetlinkAttribute::NlaGetU32(nst));
  }
  return true;
}

// Helper function to provide a string for NL80211_ATTR_SCAN_SSIDS.
bool Nl80211Message::GetScanSsidsAttribute(
    int id, vector<string> *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  ByteString rawdata;
  if (!attributes().GetRawAttributeValue(id, &rawdata) || rawdata.IsEmpty())
    return false;

  nlattr *nst = NULL;
  // |nla_for_each_attr| requires a non-const parameter even though it
  // doesn't change the data.
  nlattr *data = reinterpret_cast<nlattr *>(rawdata.GetData());
  int rem_nst;
  int len = rawdata.GetLength();

  nla_for_each_attr(nst, data, len, rem_nst) {
    value->push_back(StringFromSsid(nla_len(nst),
                                    reinterpret_cast<const uint8_t *>(
                                      nla_data(nst))).c_str());
  }
  return true;
}

// Protected members.

string Nl80211Message::GetHeaderString() const {
  char ifname[IF_NAMESIZE] = "";
  uint32_t ifindex = UINT32_MAX;
  bool ifindex_exists = attributes().GetU32AttributeValue(NL80211_ATTR_IFINDEX,
                                                          &ifindex);

  uint32_t wifi = UINT32_MAX;
  bool wifi_exists = attributes().GetU32AttributeValue(NL80211_ATTR_WIPHY,
                                                       &wifi);

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

string Nl80211Message::StringFromFrame(int attr_name) const {
  string output;
  ByteString frame_data;
  if (attributes().GetRawAttributeValue(attr_name,
                                        &frame_data) && !frame_data.IsEmpty()) {
    Nl80211Frame frame(frame_data);
    frame.ToString(&output);
  } else {
    output.append(" [no frame]");
  }

  return output;
}

// static
string Nl80211Message::StringFromKeyType(nl80211_key_type key_type) {
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
string Nl80211Message::StringFromMacAddress(const uint8_t *arg) {
  string output;

  if (!arg) {
    output = kBogusMacAddress;
    LOG(ERROR) << "|arg| parameter is NULL.";
  } else {
    StringAppendF(&output, "%02x", arg[0]);

    for (unsigned int i = 1; i < kEthernetAddressBytes ; ++i) {
      StringAppendF(&output, ":%02x", arg[i]);
    }
  }
  return output;
}

// static
string Nl80211Message::StringFromRegInitiator(__u8 initiator) {
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
string Nl80211Message::StringFromSsid(const uint8_t len,
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
string Nl80211Message::StringFromReason(uint16_t status) {
  map<uint16_t, string>::const_iterator match;
  match = reason_code_string_->find(status);
  if (match == reason_code_string_->end()) {
    string output;
    if (status < IEEE_80211::kReasonCodeMax) {
      StringAppendF(&output, "<Reserved Reason:%u>", status);
    } else {
      StringAppendF(&output, "<Unknown Reason:%u>", status);
    }
    return output;
  }
  return match->second;
}

// static
string Nl80211Message::StringFromStatus(uint16_t status) {
  map<uint16_t, string>::const_iterator match;
  match = status_code_string_->find(status);
  if (match == status_code_string_->end()) {
    string output;
    if (status < IEEE_80211::kStatusCodeMax) {
      StringAppendF(&output, "<Reserved Status:%u>", status);
    } else {
      StringAppendF(&output, "<Unknown Status:%u>", status);
    }
    return output;
  }
  return match->second;
}

ByteString Nl80211Message::Encode(uint16_t nlmsg_type) const {
  // Build netlink header.
  nlmsghdr header;
  size_t nlmsghdr_with_pad = NLMSG_ALIGN(sizeof(header));
  header.nlmsg_len = nlmsghdr_with_pad;
  header.nlmsg_type = nlmsg_type;
  header.nlmsg_flags = NLM_F_REQUEST;
  header.nlmsg_seq = sequence_number();
  header.nlmsg_pid = getpid();

  // Build genl message header.
  genlmsghdr genl_header;
  size_t genlmsghdr_with_pad = NLMSG_ALIGN(sizeof(genl_header));
  header.nlmsg_len += genlmsghdr_with_pad;
  genl_header.cmd = message_type();
  genl_header.version = 1;
  genl_header.reserved = 0;

  // Assemble attributes (padding is included by AttributeList::Encode).
  ByteString attributes = attributes_.Encode();
  header.nlmsg_len += attributes.GetLength();

  // Now that we know the total message size, build the output ByteString.
  ByteString result;

  // Netlink header + pad.
  result.Append(ByteString(reinterpret_cast<unsigned char *>(&header),
                           sizeof(header)));
  result.Resize(nlmsghdr_with_pad);  // Zero-fill pad space (if any).

  // Genl message header + pad.
  result.Append(ByteString(reinterpret_cast<unsigned char *>(&genl_header),
                           sizeof(genl_header)));
  result.Resize(nlmsghdr_with_pad + genlmsghdr_with_pad);  // Zero-fill.

  // Attributes including pad.
  result.Append(attributes);

  return result;
}

Nl80211Frame::Nl80211Frame(const ByteString &raw_frame)
  : frame_type_(kIllegalFrameType), reason_(UINT16_MAX), status_(UINT16_MAX),
    frame_(raw_frame) {
  const IEEE_80211::ieee80211_frame *frame =
      reinterpret_cast<const IEEE_80211::ieee80211_frame *>(
          frame_.GetConstData());

  // Now, let's populate the other stuff.
  if (frame_.GetLength() >= kMinimumFrameByteCount) {
    mac_from_ =
        Nl80211Message::StringFromMacAddress(&frame->destination_mac[0]);
    mac_to_ = Nl80211Message::StringFromMacAddress(&frame->source_mac[0]);
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
      reason_ = le16toh(frame->u.deauthentiate_message.reason_code);
      break;

    default:
      break;
    }
  }
}

bool Nl80211Frame::ToString(string *output) const {
  if (!output) {
    LOG(ERROR) << "NULL |output|";
    return false;
  }

  if (frame_.IsEmpty()) {
    output->append(" [no frame]");
    return true;
  }

  if (frame_.GetLength() < kMinimumFrameByteCount) {
    output->append(" [invalid frame: ");
  } else {
    StringAppendF(output, " %s -> %s", mac_from_.c_str(), mac_to_.c_str());

    switch (frame_.GetConstData()[0] & kFrameTypeMask) {
    case kAssocResponseFrameType:
      StringAppendF(output, "; AssocResponse status: %u: %s",
                    status_,
                    Nl80211Message::StringFromStatus(status_).c_str());
      break;
    case kReassocResponseFrameType:
      StringAppendF(output, "; ReassocResponse status: %u: %s",
                    status_,
                    Nl80211Message::StringFromStatus(status_).c_str());
      break;
    case kAuthFrameType:
      StringAppendF(output, "; Auth status: %u: %s",
                    status_,
                    Nl80211Message::StringFromStatus(status_).c_str());
      break;

    case kDisassocFrameType:
      StringAppendF(output, "; Disassoc reason %u: %s",
                    reason_,
                    Nl80211Message::StringFromReason(reason_).c_str());
      break;
    case kDeauthFrameType:
      StringAppendF(output, "; Deauth reason %u: %s",
                    reason_,
                    Nl80211Message::StringFromReason(reason_).c_str());
      break;

    default:
      break;
    }
    output->append(" [frame: ");
  }

  const unsigned char *frame = frame_.GetConstData();
  for (size_t i = 0; i < frame_.GetLength(); ++i) {
    StringAppendF(output, "%02x, ", frame[i]);
  }
  output->append("]");

  return true;
}

bool Nl80211Frame::IsEqual(const Nl80211Frame &other) const {
  return frame_.Equals(other.frame_);
}


//
// Specific Nl80211Message types.
//

// An Ack is not a GENL message and, as such, has no command.
const uint8_t AckMessage::kCommand = NL80211_CMD_UNSPEC;
const char AckMessage::kCommandString[] = "NL80211_ACK";

const uint8_t AssociateMessage::kCommand = NL80211_CMD_ASSOCIATE;
const char AssociateMessage::kCommandString[] = "NL80211_CMD_ASSOCIATE";

const uint8_t AuthenticateMessage::kCommand = NL80211_CMD_AUTHENTICATE;
const char AuthenticateMessage::kCommandString[] = "NL80211_CMD_AUTHENTICATE";

const uint8_t CancelRemainOnChannelMessage::kCommand =
  NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL;
const char CancelRemainOnChannelMessage::kCommandString[] =
  "NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL";

const uint8_t ConnectMessage::kCommand = NL80211_CMD_CONNECT;
const char ConnectMessage::kCommandString[] = "NL80211_CMD_CONNECT";

const uint8_t DeauthenticateMessage::kCommand = NL80211_CMD_DEAUTHENTICATE;
const char DeauthenticateMessage::kCommandString[] =
    "NL80211_CMD_DEAUTHENTICATE";

const uint8_t DeleteStationMessage::kCommand = NL80211_CMD_DEL_STATION;
const char DeleteStationMessage::kCommandString[] = "NL80211_CMD_DEL_STATION";

const uint8_t DisassociateMessage::kCommand = NL80211_CMD_DISASSOCIATE;
const char DisassociateMessage::kCommandString[] = "NL80211_CMD_DISASSOCIATE";

const uint8_t DisconnectMessage::kCommand = NL80211_CMD_DISCONNECT;
const char DisconnectMessage::kCommandString[] = "NL80211_CMD_DISCONNECT";

// An Error is not a GENL message and, as such, has no command.
const uint8_t ErrorMessage::kCommand = NL80211_CMD_UNSPEC;
const char ErrorMessage::kCommandString[] = "NL80211_ERROR";

ErrorMessage::ErrorMessage(uint32_t error)
  : Nl80211Message(kCommand, kCommandString), error_(error) {}

string ErrorMessage::ToString() const {
  string output;
  StringAppendF(&output, "NL80211_ERROR %" PRIx32 ": %s",
                error_, strerror(error_));
  return output;
}

const uint8_t FrameTxStatusMessage::kCommand = NL80211_CMD_FRAME_TX_STATUS;
const char FrameTxStatusMessage::kCommandString[] =
    "NL80211_CMD_FRAME_TX_STATUS";

const uint8_t GetRegMessage::kCommand = NL80211_CMD_GET_REG;
const char GetRegMessage::kCommandString[] = "NL80211_CMD_GET_REG";

const uint8_t JoinIbssMessage::kCommand = NL80211_CMD_JOIN_IBSS;
const char JoinIbssMessage::kCommandString[] = "NL80211_CMD_JOIN_IBSS";

const uint8_t MichaelMicFailureMessage::kCommand =
    NL80211_CMD_MICHAEL_MIC_FAILURE;
const char MichaelMicFailureMessage::kCommandString[] =
    "NL80211_CMD_MICHAEL_MIC_FAILURE";

const uint8_t NewScanResultsMessage::kCommand = NL80211_CMD_NEW_SCAN_RESULTS;
const char NewScanResultsMessage::kCommandString[] =
    "NL80211_CMD_NEW_SCAN_RESULTS";

const uint8_t NewStationMessage::kCommand = NL80211_CMD_NEW_STATION;
const char NewStationMessage::kCommandString[] = "NL80211_CMD_NEW_STATION";

const uint8_t NewWifiMessage::kCommand = NL80211_CMD_NEW_WIPHY;
const char NewWifiMessage::kCommandString[] = "NL80211_CMD_NEW_WIPHY";

// A NOOP is not a GENL message and, as such, has no command.
const uint8_t NoopMessage::kCommand = NL80211_CMD_UNSPEC;
const char NoopMessage::kCommandString[] = "NL80211_NOOP";

const uint8_t NotifyCqmMessage::kCommand = NL80211_CMD_NOTIFY_CQM;
const char NotifyCqmMessage::kCommandString[] = "NL80211_CMD_NOTIFY_CQM";

const uint8_t PmksaCandidateMessage::kCommand = NL80211_ATTR_PMKSA_CANDIDATE;
const char PmksaCandidateMessage::kCommandString[] =
  "NL80211_ATTR_PMKSA_CANDIDATE";

const uint8_t RegBeaconHintMessage::kCommand = NL80211_CMD_REG_BEACON_HINT;
const char RegBeaconHintMessage::kCommandString[] =
    "NL80211_CMD_REG_BEACON_HINT";

const uint8_t RegChangeMessage::kCommand = NL80211_CMD_REG_CHANGE;
const char RegChangeMessage::kCommandString[] = "NL80211_CMD_REG_CHANGE";

const uint8_t RemainOnChannelMessage::kCommand = NL80211_CMD_REMAIN_ON_CHANNEL;
const char RemainOnChannelMessage::kCommandString[] =
    "NL80211_CMD_REMAIN_ON_CHANNEL";

const uint8_t RoamMessage::kCommand = NL80211_CMD_ROAM;
const char RoamMessage::kCommandString[] = "NL80211_CMD_ROAM";

const uint8_t ScanAbortedMessage::kCommand = NL80211_CMD_SCAN_ABORTED;
const char ScanAbortedMessage::kCommandString[] = "NL80211_CMD_SCAN_ABORTED";

const uint8_t TriggerScanMessage::kCommand = NL80211_CMD_TRIGGER_SCAN;
const char TriggerScanMessage::kCommandString[] = "NL80211_CMD_TRIGGER_SCAN";

const uint8_t UnknownMessage::kCommand = 0xff;
const char UnknownMessage::kCommandString[] = "<Unknown Message Type>";

const uint8_t UnprotDeauthenticateMessage::kCommand =
    NL80211_CMD_UNPROT_DEAUTHENTICATE;
const char UnprotDeauthenticateMessage::kCommandString[] =
    "NL80211_CMD_UNPROT_DEAUTHENTICATE";

const uint8_t UnprotDisassociateMessage::kCommand =
    NL80211_CMD_UNPROT_DISASSOCIATE;
const char UnprotDisassociateMessage::kCommandString[] =
    "NL80211_CMD_UNPROT_DISASSOCIATE";

//
// Factory class.
//

Nl80211Message *Nl80211MessageFactory::CreateMessage(nlmsghdr *msg) {
  if (!msg) {
    LOG(ERROR) << "NULL |msg| parameter";
    return NULL;
  }

  scoped_ptr<Nl80211Message> message;
  void *payload = nlmsg_data(msg);

  if (msg->nlmsg_type == NLMSG_NOOP) {
    SLOG(WiFi, 6) << "Creating a NOP message";
    message.reset(new NoopMessage());
  } else if (msg->nlmsg_type == NLMSG_ERROR) {
    uint32_t error_code = *(reinterpret_cast<uint32_t *>(payload));
    if (error_code) {
      SLOG(WiFi, 6) << "Creating an ERROR message:" << error_code;
      message.reset(new ErrorMessage(error_code));
    } else {
      SLOG(WiFi, 6) << "Creating an ACK message";
      message.reset(new AckMessage());
    }
  } else {
    SLOG(WiFi, 6) << "Creating a Regular message";
    genlmsghdr *gnlh = reinterpret_cast<genlmsghdr *>(payload);

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
      case DeleteStationMessage::kCommand:
        message.reset(new DeleteStationMessage()); break;
      case DisassociateMessage::kCommand:
        message.reset(new DisassociateMessage()); break;
      case DisconnectMessage::kCommand:
        message.reset(new DisconnectMessage()); break;
      case FrameTxStatusMessage::kCommand:
        message.reset(new FrameTxStatusMessage()); break;
      case GetRegMessage::kCommand:
        message.reset(new GetRegMessage()); break;
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
    }

    if (!message->InitFromNlmsg(msg)) {
      LOG(ERROR) << "Message did not initialize properly";
      return NULL;
    }
  }

  return message.release();
}

Nl80211MessageDataCollector *
    Nl80211MessageDataCollector::GetInstance() {
  return g_datacollector.Pointer();
}

Nl80211MessageDataCollector::Nl80211MessageDataCollector() {
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

void Nl80211MessageDataCollector::CollectDebugData(
    const Nl80211Message &message, nlmsghdr *msg) {
  if (!msg) {
    LOG(ERROR) << "NULL |msg| parameter";
    return;
  }

  bool doit = false;

  map<uint8_t, bool>::const_iterator node;
  node = need_to_print.find(message.message_type());
  if (node != need_to_print.end())
    doit = node->second;

  if (doit) {
    LOG(INFO) << "@@const unsigned char "
               << "k" << message.message_type_string()
               << "[] = {";

    int payload_bytes = nlmsg_datalen(msg);

    size_t bytes = nlmsg_total_size(payload_bytes);
    unsigned char *rawdata = reinterpret_cast<unsigned char *>(msg);
    for (size_t i = 0; i < bytes; ++i) {
      LOG(INFO) << "  0x"
                 << std::hex << std::setfill('0') << std::setw(2)
                 << + rawdata[i] << ",";
    }
    LOG(INFO) << "};";
    need_to_print[message.message_type()] = false;
  }
}

}  // namespace shill.
