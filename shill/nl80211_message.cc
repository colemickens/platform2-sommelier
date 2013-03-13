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

#include <limits.h>
#include <netlink/attr.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/format_macros.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>

#include "shill/attribute_list.h"
#include "shill/ieee80211.h"
#include "shill/logging.h"
#include "shill/netlink_attribute.h"
#include "shill/refptr_types.h"

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

const uint8_t Nl80211Frame::kMinimumFrameByteCount = 26;
const uint8_t Nl80211Frame::kFrameTypeMask = 0xfc;

const uint32_t NetlinkMessage::kBroadcastSequenceNumber = 0;
const uint16_t NetlinkMessage::kIllegalMessageType = UINT16_MAX;

const char Nl80211Message::kBogusMacAddress[] = "XX:XX:XX:XX:XX:XX";
const unsigned int Nl80211Message::kEthernetAddressBytes = 6;
const char Nl80211Message::kMessageTypeString[] = "nl80211";
map<uint16_t, string> *Nl80211Message::reason_code_string_ = NULL;
map<uint16_t, string> *Nl80211Message::status_code_string_ = NULL;
uint16_t Nl80211Message::nl80211_message_type_ = kIllegalMessageType;

// NetlinkMessage

ByteString NetlinkMessage::EncodeHeader(uint32_t sequence_number) {
  ByteString result;
  if (message_type_ == kIllegalMessageType) {
    LOG(ERROR) << "Message type not set";
    return result;
  }
  sequence_number_ = sequence_number;
  if (sequence_number_ == kBroadcastSequenceNumber) {
    LOG(ERROR) << "Couldn't get a legal sequence number";
    return result;
  }

  // Build netlink header.
  nlmsghdr header;
  size_t nlmsghdr_with_pad = NLMSG_ALIGN(sizeof(header));
  header.nlmsg_len = nlmsghdr_with_pad;
  header.nlmsg_type = message_type_;
  header.nlmsg_flags = NLM_F_REQUEST | flags_;
  header.nlmsg_seq = sequence_number_;
  header.nlmsg_pid = getpid();

  // Netlink header + pad.
  result.Append(ByteString(reinterpret_cast<unsigned char *>(&header),
                           sizeof(header)));
  result.Resize(nlmsghdr_with_pad);  // Zero-fill pad space (if any).
  return result;
}

bool NetlinkMessage::InitAndStripHeader(ByteString *input) {
  if (!input) {
    LOG(ERROR) << "NULL input";
    return false;
  }
  if (input->GetLength() < sizeof(nlmsghdr)) {
    LOG(ERROR) << "Insufficient input to extract nlmsghdr";
    return false;
  }

  // Read the nlmsghdr.
  nlmsghdr *header = reinterpret_cast<nlmsghdr *>(input->GetData());
  message_type_ = header->nlmsg_type;
  flags_ = header->nlmsg_flags;
  sequence_number_ = header->nlmsg_seq;

  // Strip the nlmsghdr.
  input->RemovePrefix(NLMSG_ALIGN(sizeof(struct nlmsghdr)));
  return true;
}

bool NetlinkMessage::InitFromNlmsg(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "Null |const_msg| parameter";
    return false;
  }
  ByteString message(reinterpret_cast<const unsigned char *>(const_msg),
                     const_msg->nlmsg_len);
  if (!InitAndStripHeader(&message)) {
    return false;
  }
  return true;
}

// static
void NetlinkMessage::PrintBytes(int log_level, const unsigned char *buf,
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

// ErrorAckMessage.

const uint16_t ErrorAckMessage::kMessageType = NLMSG_ERROR;

bool ErrorAckMessage::InitFromNlmsg(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "Null |const_msg| parameter";
    return false;
  }
  ByteString message(reinterpret_cast<const unsigned char *>(const_msg),
                     const_msg->nlmsg_len);
  if (!InitAndStripHeader(&message)) {
    return false;
  }

  // Get the error code from the payload.
  error_ = *(reinterpret_cast<const uint32_t *>(message.GetConstData()));
  return true;
}

ByteString ErrorAckMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR) << "We're not supposed to send errors or Acks to the kernel";
  return ByteString();
}

string ErrorAckMessage::ToString() const {
  string output;
  if (error()) {
    StringAppendF(&output, "NL80211_ERROR 0x%" PRIx32 ": %s",
                  -error_, strerror(-error_));
  } else {
    StringAppendF(&output, "ACK");
  }
  return output;
}

void ErrorAckMessage::Print(int log_level) const {
  SLOG(WiFi, log_level) << ToString();
}

// NoopMessage.

const uint16_t NoopMessage::kMessageType = NLMSG_NOOP;

ByteString NoopMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR) << "We're not supposed to send NOOP to the kernel";
  return ByteString();
}

void NoopMessage::Print(int log_level) const {
  SLOG(WiFi, log_level) << ToString();
}

// DoneMessage.

const uint16_t DoneMessage::kMessageType = NLMSG_DONE;

ByteString DoneMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR)
      << "We're not supposed to send Done messages (are we?) to the kernel";
  return ByteString();
}

void DoneMessage::Print(int log_level) const {
  SLOG(WiFi, log_level) << ToString();
}

// OverrunMessage.

const uint16_t OverrunMessage::kMessageType = NLMSG_OVERRUN;

ByteString OverrunMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR) << "We're not supposed to send Overruns to the kernel";
  return ByteString();
}

void OverrunMessage::Print(int log_level) const {
  SLOG(WiFi, log_level) << ToString();
}

// UnknownMessage.

ByteString UnknownMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR) << "We're not supposed to send UNKNOWN messages to the kernel";
  return ByteString();
}

void UnknownMessage::Print(int log_level) const {
  int total_bytes = message_body_.GetLength();
  const uint8_t *const_data = message_body_.GetConstData();

  string output = StringPrintf("%d bytes:", total_bytes);
  for (int i = 0; i < total_bytes; ++i) {
    StringAppendF(&output, " 0x%02x", const_data[i]);
  }
  SLOG(WiFi, log_level) << output;
}

// GenericNetlinkMessage

ByteString GenericNetlinkMessage::EncodeHeader(uint32_t sequence_number) {
  // Build nlmsghdr.
  ByteString result(NetlinkMessage::EncodeHeader(sequence_number));
  if (result.GetLength() == 0) {
    LOG(ERROR) << "Couldn't encode message header.";
    return result;
  }

  // Build and append the genl message header.
  genlmsghdr genl_header;
  genl_header.cmd = command();
  genl_header.version = 1;
  genl_header.reserved = 0;

  ByteString genl_header_string(
      reinterpret_cast<unsigned char *>(&genl_header), sizeof(genl_header));
  size_t genlmsghdr_with_pad = NLMSG_ALIGN(sizeof(genl_header));
  genl_header_string.Resize(genlmsghdr_with_pad);  // Zero-fill.

  nlmsghdr *pheader = reinterpret_cast<nlmsghdr *>(result.GetData());
  pheader->nlmsg_len += genlmsghdr_with_pad;
  result.Append(genl_header_string);
  return result;
}

ByteString GenericNetlinkMessage::Encode(uint32_t sequence_number) {
  ByteString result(EncodeHeader(sequence_number));
  if (result.GetLength() == 0) {
    LOG(ERROR) << "Couldn't encode message header.";
    return result;
  }

  // Build and append attributes (padding is included by
  // AttributeList::Encode).
  ByteString attribute_string = attributes_->Encode();

  // Need to re-calculate |header| since |Append|, above, moves the data.
  nlmsghdr *pheader = reinterpret_cast<nlmsghdr *>(result.GetData());
  pheader->nlmsg_len += attribute_string.GetLength();
  result.Append(attribute_string);

  return result;
}

bool GenericNetlinkMessage::InitAndStripHeader(ByteString *input) {
  if (!input) {
    LOG(ERROR) << "NULL input";
    return false;
  }
  if (!NetlinkMessage::InitAndStripHeader(input)) {
    return false;
  }

  // Read the genlmsghdr.
  genlmsghdr *gnlh = reinterpret_cast<genlmsghdr *>(input->GetData());
  if (command_ != gnlh->cmd) {
    LOG(WARNING) << "This object thinks it's a " << command_
                 << " but the message thinks it's a " << gnlh->cmd;
  }

  // Strip the genlmsghdr.
  input->RemovePrefix(NLMSG_ALIGN(sizeof(struct genlmsghdr)));
  return true;
}

void GenericNetlinkMessage::Print(int log_level) const {
  SLOG(WiFi, log_level) << StringPrintf("Message %s (%d)",
                                        command_string(),
                                        command());
  attributes_->Print(log_level, 1);
}

// Nl80211Message

// static
void Nl80211Message::SetMessageType(uint16_t message_type) {
  if (message_type == NetlinkMessage::kIllegalMessageType) {
    LOG(FATAL) << "Absolutely need a legal message type for Nl80211 messages.";
  }
  nl80211_message_type_ = message_type;
}

bool Nl80211Message::InitFromNlmsg(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "Null |msg| parameter";
    return false;
  }
  ByteString message(reinterpret_cast<const unsigned char *>(const_msg),
                     const_msg->nlmsg_len);

  if (!InitAndStripHeader(&message)) {
    return false;
  }

  // Attributes.
  // Parse the attributes from the nl message payload into the 'tb' array.
  nlattr *tb[NL80211_ATTR_MAX + 1];
  nla_parse(tb, NL80211_ATTR_MAX,
            reinterpret_cast<nlattr *>(message.GetData()), message.GetLength(),
            NULL);

  for (int i = 0; i < NL80211_ATTR_MAX + 1; ++i) {
    if (tb[i]) {
      // TODO(wdg): When Nl80211Messages instantiate their own attributes,
      // this call should, instead, call |SetAttributeFromNlAttr|.
      attributes_->CreateAndInitAttribute(
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

// Helper function to provide a string for a MAC address.
bool Nl80211Message::GetMacAttributeString(int id, string *value) const {
  if (!value) {
    LOG(ERROR) << "Null |value| parameter";
    return false;
  }

  ByteString data;
  if (!const_attributes()->GetRawAttributeValue(id, &data)) {
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

  AttributeListConstRefPtr frequency_list;
  if (!const_attributes()->ConstGetNestedAttributeList(
      NL80211_ATTR_SCAN_FREQUENCIES, &frequency_list) || !frequency_list) {
    LOG(ERROR) << "Couldn't get NL80211_ATTR_SCAN_FREQUENCIES attribute";
    return false;
  }

  // Assume IDs for the nested attribute array are linear starting from 1.
  // Currently, that is enforced in the input to the nested attribute.
  uint32_t freq;
  int i = 1;
  while (frequency_list->GetU32AttributeValue(i, &freq)) {
    value->push_back(freq);
    ++i;
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

  AttributeListConstRefPtr ssid_list;
  if (!const_attributes()->ConstGetNestedAttributeList(
      NL80211_ATTR_SCAN_SSIDS, &ssid_list) || !ssid_list) {
    LOG(ERROR) << "Couldn't get NL80211_ATTR_SCAN_SSIDS attribute";
    return false;
  }

  // Assume IDs for the nested attribute array are linear starting from 1.
  // Currently, that is enforced in the input to the nested attribute.
  string ssid;
  int i = 1;
  while (ssid_list->GetStringAttributeValue(i, &ssid)) {
    value->push_back(ssid);
    ++i;
  }
  return true;
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


// Control Message

const uint16_t ControlNetlinkMessage::kMessageType = GENL_ID_CTRL;

bool ControlNetlinkMessage::InitFromNlmsg(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "Null |msg| parameter";
    return false;
  }
  ByteString message(reinterpret_cast<const unsigned char *>(const_msg),
                     const_msg->nlmsg_len);

  if (!InitAndStripHeader(&message)) {
    return false;
  }

  // Attributes.
  // Parse the attributes from the nl message payload into the 'tb' array.
  nlattr *tb[CTRL_ATTR_MAX + 1];
  nla_parse(tb, CTRL_ATTR_MAX,
            reinterpret_cast<nlattr *>(message.GetData()), message.GetLength(),
            NULL);

  for (int i = 0; i < CTRL_ATTR_MAX + 1; ++i) {
    if (tb[i]) {
      // TODO(wdg): When Nl80211Messages instantiate their own attributes,
      // this call should, instead, call |SetAttributeFromNlAttr|.
      attributes_->CreateAndInitAttribute(
          i, tb[i], Bind(&NetlinkAttribute::NewControlAttributeFromId));
    }
  }
  return true;
}

// Specific Control types.

const uint8_t NewFamilyMessage::kCommand = CTRL_CMD_NEWFAMILY;
const char NewFamilyMessage::kCommandString[] = "CTRL_CMD_NEWFAMILY";

const uint8_t GetFamilyMessage::kCommand = CTRL_CMD_GETFAMILY;
const char GetFamilyMessage::kCommandString[] = "CTRL_CMD_GETFAMILY";

// static
NetlinkMessage *ControlNetlinkMessage::CreateMessage(
    const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "NULL |const_msg| parameter";
    return NULL;
  }
  // Casting away constness since, while nlmsg_data doesn't change its
  // parameter, it also doesn't declare its paramenter as const.
  nlmsghdr *msg = const_cast<nlmsghdr *>(const_msg);
  void *payload = nlmsg_data(msg);
  genlmsghdr *gnlh = reinterpret_cast<genlmsghdr *>(payload);

  switch (gnlh->cmd) {
    case NewFamilyMessage::kCommand:
      return new NewFamilyMessage();
    case GetFamilyMessage::kCommand:
      return new GetFamilyMessage();
    default:
      LOG(WARNING) << "Unknown/unhandled netlink control message " << gnlh->cmd;
      break;
  }
  return NULL;
}

// Nl80211Frame

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

const uint8_t GetScanMessage::kCommand = NL80211_CMD_GET_SCAN;
const char GetScanMessage::kCommandString[] = "NL80211_CMD_GET_SCAN";

const uint8_t TriggerScanMessage::kCommand = NL80211_CMD_TRIGGER_SCAN;
const char TriggerScanMessage::kCommandString[] = "NL80211_CMD_TRIGGER_SCAN";

const uint8_t UnprotDeauthenticateMessage::kCommand =
    NL80211_CMD_UNPROT_DEAUTHENTICATE;
const char UnprotDeauthenticateMessage::kCommandString[] =
    "NL80211_CMD_UNPROT_DEAUTHENTICATE";

const uint8_t UnprotDisassociateMessage::kCommand =
    NL80211_CMD_UNPROT_DISASSOCIATE;
const char UnprotDisassociateMessage::kCommandString[] =
    "NL80211_CMD_UNPROT_DISASSOCIATE";

// static
NetlinkMessage *Nl80211Message::CreateMessage(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "NULL |const_msg| parameter";
    return NULL;
  }
  // Casting away constness since, while nlmsg_data doesn't change its
  // parameter, it also doesn't declare its paramenter as const.
  nlmsghdr *msg = const_cast<nlmsghdr *>(const_msg);
  void *payload = nlmsg_data(msg);
  genlmsghdr *gnlh = reinterpret_cast<genlmsghdr *>(payload);
  scoped_ptr<NetlinkMessage> message;

  switch (gnlh->cmd) {
    case AssociateMessage::kCommand:
      return new AssociateMessage();
    case AuthenticateMessage::kCommand:
      return new AuthenticateMessage();
    case CancelRemainOnChannelMessage::kCommand:
      return new CancelRemainOnChannelMessage();
    case ConnectMessage::kCommand:
      return new ConnectMessage();
    case DeauthenticateMessage::kCommand:
      return new DeauthenticateMessage();
    case DeleteStationMessage::kCommand:
      return new DeleteStationMessage();
    case DisassociateMessage::kCommand:
      return new DisassociateMessage();
    case DisconnectMessage::kCommand:
      return new DisconnectMessage();
    case FrameTxStatusMessage::kCommand:
      return new FrameTxStatusMessage();
    case GetRegMessage::kCommand:
      return new GetRegMessage();
    case JoinIbssMessage::kCommand:
      return new JoinIbssMessage();
    case MichaelMicFailureMessage::kCommand:
      return new MichaelMicFailureMessage();
    case NewScanResultsMessage::kCommand:
      return new NewScanResultsMessage();
    case NewStationMessage::kCommand:
      return new NewStationMessage();
    case NewWifiMessage::kCommand:
      return new NewWifiMessage();
    case NotifyCqmMessage::kCommand:
      return new NotifyCqmMessage();
    case PmksaCandidateMessage::kCommand:
      return new PmksaCandidateMessage();
    case RegBeaconHintMessage::kCommand:
      return new RegBeaconHintMessage();
    case RegChangeMessage::kCommand:
      return new RegChangeMessage();
    case RemainOnChannelMessage::kCommand:
      return new RemainOnChannelMessage();
    case RoamMessage::kCommand:
      return new RoamMessage();
    case ScanAbortedMessage::kCommand:
      return new ScanAbortedMessage();
    case TriggerScanMessage::kCommand:
      return new TriggerScanMessage();
    case UnprotDeauthenticateMessage::kCommand:
      return new UnprotDeauthenticateMessage();
    case UnprotDisassociateMessage::kCommand:
      return new UnprotDisassociateMessage();
    default:
      LOG(WARNING) << "Unknown/unhandled netlink nl80211 message " << gnlh->cmd;
      break;
  }
  return NULL;
}

//
// Factory class.
//

bool NetlinkMessageFactory::AddFactoryMethod(uint16_t message_type,
                                             FactoryMethod factory) {
  if (ContainsKey(factories_, message_type)) {
    LOG(WARNING) << "Message type " << message_type << " already exists.";
    return false;
  }
  if (message_type == NetlinkMessage::kIllegalMessageType) {
    LOG(ERROR) << "Not installing factory for illegal message type.";
    return false;
  }
  factories_[message_type] = factory;
  return true;
}

NetlinkMessage *NetlinkMessageFactory::CreateMessage(
    const nlmsghdr *const_msg) const {
  if (!const_msg) {
    LOG(ERROR) << "NULL |const_msg| parameter";
    return NULL;
  }

  scoped_ptr<NetlinkMessage> message;

  if (const_msg->nlmsg_type == NoopMessage::kMessageType) {
    message.reset(new NoopMessage());
  } else if (const_msg->nlmsg_type == DoneMessage::kMessageType) {
    message.reset(new DoneMessage());
  } else if (const_msg->nlmsg_type == OverrunMessage::kMessageType) {
    message.reset(new OverrunMessage());
  } else if (const_msg->nlmsg_type == ErrorAckMessage::kMessageType) {
    message.reset(new ErrorAckMessage());
  } else if (ContainsKey(factories_, const_msg->nlmsg_type)) {
    map<uint16_t, FactoryMethod>::const_iterator factory;
    factory = factories_.find(const_msg->nlmsg_type);
    message.reset(factory->second.Run(const_msg));
  }

  // If no factory exists for this message _or_ if a factory exists but it
  // failed, there'll be no message.  Handle either of those cases, by
  // creating an |UnknownMessage|.
  if (!message) {
    // Casting away constness since, while nlmsg_data doesn't change its
    // parameter, it also doesn't declare its paramenter as const.
    nlmsghdr *msg = const_cast<nlmsghdr *>(const_msg);
    ByteString payload(reinterpret_cast<char *>(nlmsg_data(msg)),
                       nlmsg_datalen(msg));
    message.reset(new UnknownMessage(msg->nlmsg_type, payload));
  }

  if (!message->InitFromNlmsg(const_msg)) {
    LOG(ERROR) << "Message did not initialize properly";
    return NULL;
  }

  return message.release();
}

//
// Data Collector
//

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
  node = need_to_print.find(message.command());
  if (node != need_to_print.end())
    doit = node->second;

  if (doit) {
    LOG(INFO) << "@@const unsigned char "
               << "k" << message.command_string()
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
    need_to_print[message.command()] = false;
  }
}

}  // namespace shill.
