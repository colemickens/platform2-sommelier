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

#include <iomanip>
#include <string>

#include <base/format_macros.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>

#include "shill/attribute_list.h"
#include "shill/ieee80211.h"
#include "shill/logging.h"
#include "shill/netlink_socket.h"
#include "shill/nl80211_attribute.h"
#include "shill/scope_logger.h"

using base::LazyInstance;
using base::StringAppendF;
using base::StringPrintf;
using std::map;
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
      attributes_.CreateAndInitFromNlAttr(static_cast<enum nl80211_attrs>(i),
                                          tb[i]);
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

// Helper function to provide a string for a MAC address.
bool Nl80211Message::GetMacAttributeString(nl80211_attrs id,
                                           string *value) const {
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
    nl80211_attrs id, vector<uint32_t> *value) const {
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
    value->push_back(Nl80211Attribute::NlaGetU32(nst));
  }
  return true;
}

// Helper function to provide a string for NL80211_ATTR_SCAN_SSIDS.
bool Nl80211Message::GetScanSsidsAttribute(
    nl80211_attrs id, vector<string> *value) const {
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

string Nl80211Message::StringFromFrame(nl80211_attrs attr_name) const {
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

string Nl80211Message::GenericToString() const {
  string output;
  StringAppendF(&output, "Message %s (%d)\n",
                message_type_string(), message_type());
  StringAppendF(&output, "%s", attributes_.ToString().c_str());
  return output;
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

string AckMessage::ToString() const {
  return "NL80211_ACK";
}

const uint8_t AssociateMessage::kCommand = NL80211_CMD_ASSOCIATE;
const char AssociateMessage::kCommandString[] = "NL80211_CMD_ASSOCIATE";

string AssociateMessage::ToString() const {
  string output(GetHeaderString());
  output.append("assoc");
  if (attributes().GetRawAttributeValue(NL80211_ATTR_FRAME, NULL))
    output.append(StringFromFrame(NL80211_ATTR_FRAME));
  else if (attributes().IsFlagAttributeTrue(NL80211_ATTR_TIMED_OUT))
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
  if (attributes().GetRawAttributeValue(NL80211_ATTR_FRAME, NULL)) {
    output.append(StringFromFrame(NL80211_ATTR_FRAME));
  } else {
    output.append(attributes().IsFlagAttributeTrue(NL80211_ATTR_TIMED_OUT) ?
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
                (attributes().GetU32AttributeValue(NL80211_ATTR_WIPHY_FREQ,
                                                   &freq) ?  0 : freq),
                (attributes().GetU64AttributeValue(NL80211_ATTR_COOKIE,
                                                   &cookie) ?  0 : cookie));
  return output;
}

const uint8_t ConnectMessage::kCommand = NL80211_CMD_CONNECT;
const char ConnectMessage::kCommandString[] = "NL80211_CMD_CONNECT";

string ConnectMessage::ToString() const {
  string output(GetHeaderString());

  uint16_t status = UINT16_MAX;

  if (!attributes().GetU16AttributeValue(NL80211_ATTR_STATUS_CODE, &status)) {
    output.append("unknown connect status");
  } else if (status == 0) {
    output.append("connected");
  } else {
    output.append("failed to connect");
  }

  if (attributes().GetRawAttributeValue(NL80211_ATTR_MAC, NULL)) {
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
                ((attributes().IsFlagAttributeTrue(
                    NL80211_ATTR_DISCONNECTED_BY_AP)) ?
                 "(by AP)" : "(local request)"));

  uint16_t reason = UINT16_MAX;
  if (attributes().GetU16AttributeValue(NL80211_ATTR_REASON_CODE, &reason)) {
    StringAppendF(&output, " reason: %u: %s",
                  reason, StringFromReason(reason).c_str());
  }
  return output;
}

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

string FrameTxStatusMessage::ToString() const {
  string output(GetHeaderString());
  uint64_t cookie = UINT64_MAX;
  attributes().GetU64AttributeValue(NL80211_ATTR_COOKIE, &cookie);

  StringAppendF(&output, "mgmt TX status (cookie %" PRIx64 "): %s", cookie,
                (attributes().IsFlagAttributeTrue(NL80211_ATTR_ACK) ?
                 "acked" : "no ack"));
  return output;
}

const uint8_t GetRegMessage::kCommand = NL80211_CMD_GET_REG;
const char GetRegMessage::kCommandString[] = "NL80211_CMD_GET_REG";


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

  if (attributes().GetRawAttributeValue(NL80211_ATTR_MAC, NULL)) {
    string mac;
    GetMacAttributeString(NL80211_ATTR_MAC, &mac);
    StringAppendF(&output, " source MAC address %s", mac.c_str());
  }

  {
    ByteString rawdata;
    if (attributes().GetRawAttributeValue(NL80211_ATTR_KEY_SEQ,
                                          &rawdata) &&
        rawdata.GetLength() == Nl80211Message::kEthernetAddressBytes) {
      const unsigned char *seq = rawdata.GetConstData();
      StringAppendF(&output, " seq=%02x%02x%02x%02x%02x%02x",
                    seq[0], seq[1], seq[2], seq[3], seq[4], seq[5]);
    }
  }
  uint32_t key_type_val = UINT32_MAX;
  if (attributes().GetU32AttributeValue(NL80211_ATTR_KEY_TYPE, &key_type_val)) {
    nl80211_key_type key_type = static_cast<nl80211_key_type>(key_type_val);
    StringAppendF(&output, " Key Type %s", StringFromKeyType(key_type).c_str());
  }

  uint8_t key_index = UINT8_MAX;
  if (attributes().GetU8AttributeValue(NL80211_ATTR_KEY_IDX, &key_index)) {
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
  attributes().GetStringAttributeValue(NL80211_ATTR_WIPHY_NAME, &wifi_name);
  StringAppendF(&output, "renamed to %s", wifi_name.c_str());
  return output;
}

// A NOOP is not a GENL message and, as such, has no command.
const uint8_t NoopMessage::kCommand = NL80211_CMD_UNSPEC;
const char NoopMessage::kCommandString[] = "NL80211_NOOP";

string NoopMessage::ToString() const {
  return "NL80211_NOOP";
}

const uint8_t NotifyCqmMessage::kCommand = NL80211_CMD_NOTIFY_CQM;
const char NotifyCqmMessage::kCommandString[] = "NL80211_CMD_NOTIFY_CQM";

string NotifyCqmMessage::ToString() const {
  // TODO(wdg): use attributes().GetNestedAttributeValue()...
  static const nla_policy kCqmValidationPolicy[NL80211_ATTR_CQM_MAX + 1] = {
    { NLA_U32, 0, 0 },  // Who Knows?
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_THOLD]
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_HYST]
    { NLA_U32, 0, 0 },  // [NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]
  };

  string output(GetHeaderString());
  output.append("connection quality monitor event: ");

  const Nl80211RawAttribute *attribute =
      attributes().GetRawAttribute(NL80211_ATTR_CQM);
  if (!attribute) {
    output.append("missing data!");
    return output;
  }

  const nlattr *const_data = attribute->data();
  // Note that |nla_parse_nested| doesn't change |const_data| but doesn't
  // declare itself as 'const', either.  Hence the cast.
  nlattr *cqm_attr = const_cast<nlattr *>(const_data);

  nlattr *cqm[NL80211_ATTR_CQM_MAX + 1];
  if (!cqm_attr ||
      nla_parse_nested(cqm, NL80211_ATTR_CQM_MAX, cqm_attr,
                       const_cast<nla_policy *>(kCqmValidationPolicy))) {
    output.append("missing data!");
    return output;
  }
  if (cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]) {
    nl80211_cqm_rssi_threshold_event rssi_event =
        static_cast<nl80211_cqm_rssi_threshold_event>(
          Nl80211Attribute::NlaGetU32(
              cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]));
    if (rssi_event == NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH)
      output.append("RSSI went above threshold");
    else
      output.append("RSSI went below threshold");
  } else if (cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT] &&
       attributes().GetRawAttributeValue(NL80211_ATTR_MAC, NULL)) {
    string mac;
    GetMacAttributeString(NL80211_ATTR_MAC, &mac);
    StringAppendF(&output, "peer %s didn't ACK %" PRIu32 " packets",
                  mac.c_str(),
                  Nl80211Attribute::NlaGetU32(
                      cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT]));
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
  attributes().GetU32AttributeValue(NL80211_ATTR_WIPHY, &wiphy_idx);

  const Nl80211RawAttribute *freq_before =
      attributes().GetRawAttribute(NL80211_ATTR_FREQ_BEFORE);
  if (!freq_before)
    return "";
  const nlattr *const_before = freq_before->data();
  ieee80211_beacon_channel chan_before_beacon;
  memset(&chan_before_beacon, 0, sizeof(chan_before_beacon));
  if (ParseBeaconHintChan(const_before, &chan_before_beacon))
    return "";

  const Nl80211RawAttribute *freq_after =
      attributes().GetRawAttribute(NL80211_ATTR_FREQ_AFTER);
  if (!freq_after)
    return "";

  const nlattr *const_after = freq_after->data();
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

  // Note that |nla_parse_nested| doesn't change its parameters but doesn't
  // declare itself as 'const', either.  Hence the cast.
  if (nla_parse_nested(tb_freq,
                       NL80211_FREQUENCY_ATTR_MAX,
                       const_cast<nlattr *>(tb),
                       const_cast<nla_policy *>(kBeaconFreqPolicy)))
    return -EINVAL;

  chan->center_freq = Nl80211Attribute::NlaGetU32(
      tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);

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
  attributes().GetU8AttributeValue(NL80211_ATTR_REG_TYPE, &reg_type);

  uint32_t initiator = UINT32_MAX;
  attributes().GetU32AttributeValue(NL80211_ATTR_REG_INITIATOR, &initiator);

  uint32_t wifi = UINT32_MAX;
  bool wifi_exists = attributes().GetU32AttributeValue(NL80211_ATTR_WIPHY,
                                                       &wifi);

  string alpha2 = "<None>";
  attributes().GetStringAttributeValue(NL80211_ATTR_REG_ALPHA2, &alpha2);

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
  attributes().GetU32AttributeValue(NL80211_ATTR_WIPHY_FREQ, &wifi_freq);

  uint32_t duration = UINT32_MAX;
  attributes().GetU32AttributeValue(NL80211_ATTR_DURATION, &duration);

  uint64_t cookie = UINT64_MAX;
  attributes().GetU64AttributeValue(NL80211_ATTR_COOKIE, &cookie);

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

  if (attributes().GetRawAttributeValue(NL80211_ATTR_MAC, NULL)) {
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
