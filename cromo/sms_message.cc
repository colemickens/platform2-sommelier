// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sms_message.h"

#include <glog/logging.h>

#include "utilities.h"

static const uint8_t kMsgTypeMask          = 0x03;
static const uint8_t kMsgTypeDeliver       = 0x00;
// udhi is "User Data Header Indicator"
static const uint8_t kTpUdhi               = 0x40;

static const uint8_t kTypeOfAddrNumMask    = 0x70;
static const uint8_t kTypeOfAddrNumIntl    = 0x10;
static const uint8_t kTypeOfAddrNumAlpha   = 0x50;
static const uint8_t kTypeOfAddrIntlE164   = 0x91;

// SMS user data header information element IDs
static const uint8_t kConcatenatedSms8bit = 0x00;
static const uint8_t kConcatenatedSms16bit = 0x08;

static const uint8_t kSmscTimestampLen = 7;
static const size_t  kMinPduLen = 7 + kSmscTimestampLen;

static char NibbleToChar(uint8_t nibble) {
  switch (nibble) {
    case 0: return '0';
    case 1: return '1';
    case 2: return '2';
    case 3: return '3';
    case 4: return '4';
    case 5: return '5';
    case 6: return '6';
    case 7: return '7';
    case 8: return '8';
    case 9: return '9';
    case 10: return '*';
    case 11: return '#';
    case 12: return 'a';
    case 13: return 'b';
    case 14: return 'c';
    case 15: return '\0';  // padding nibble
  }
  return '\0';
}

// Convert an array of octets into a BCD string. Each octet consists
// of two nibbles which are converted to hex characters. Those hex
// characters are the digits of the BCD string. The lower nibble is
// the more significant digit.
static std::string SemiOctetsToBcdString(const uint8_t* octets,
                                         int num_octets) {
  std::string bcd;

  for (int i = 0; i < num_octets; ++i) {
    char first = NibbleToChar(octets[i] & 0xf);
    char second = NibbleToChar((octets[i] >> 4) & 0xf);

    if (first != '\0')
      bcd += first;
    if (second != '\0')
      bcd += second;
  }
  return bcd;
}

SmsMessageFragment* SmsMessageFragment::CreateFragment(const uint8_t* pdu,
                                                       size_t pdu_len,
                                                       int index) {
  // Make sure the PDU is of a valid size
  if (pdu_len < kMinPduLen) {
    LOG(INFO) << "PDU too short: have " << pdu_len << " need " << kMinPduLen;
    return NULL;
  }

#if 0
  {
    std::string hexpdu;
    const char *hex="0123456789abcdef";
    for (unsigned int i = 0 ; i < pdu_len ; i++) {
      hexpdu += hex[pdu[i] >> 4];
      hexpdu += hex[pdu[i] & 0xf];
    }
    LOG(INFO) << "PDU: " << hexpdu;
  }
#endif

  // Format of message:
  //
  //  1 octet  - length of SMSC information in octets, including type field
  //  1 octet  - type of address of SMSC (value 0x91 is international E.164)
  //  variable - SMSC address
  //  1 octet  - first octet of SMS-DELIVER (value = 0x04)
  //  1 octet  - length of sender address in decimal digits (semi-octets)
  //  1 octet  - type of sender address (value 0x91 is international E.164)
  //  variable - sender address
  //  1 octet  - protocol identifier
  //  1 octet  - data coding scheme
  //  7 octets - SMSC timestamp
  //  1 octet  - user data length (in septets for GSM7, else octets)
  //  variable (0 or more octets) user data header
  //  variable - user data (body of message)

  // Do a bunch of validity tests first so we can bail out early
  // if we're not able to handle the PDU. We first check the validity
  // of all length fields and make sure the PDU length is consistent
  // with those values.

  uint8_t smsc_addr_num_octets = pdu[0];
  uint8_t variable_length_items = smsc_addr_num_octets;

  if (pdu_len < variable_length_items + kMinPduLen) {
    LOG(INFO) << "PDU too short: have " << pdu_len << " need "
              << variable_length_items + kMinPduLen;
    return NULL;
  }

  // where in the PDU the actual SMS protocol message begins
  uint8_t msg_start_offset = 1 + smsc_addr_num_octets;
  uint8_t sender_addr_num_digits = pdu[msg_start_offset + 1];
  // round the sender address length up to an even number of semi-octets,
  // and thus an integral number of octets
  uint8_t sender_addr_num_octets = (sender_addr_num_digits + 1) >> 1;
  variable_length_items += sender_addr_num_octets;
  if (pdu_len < variable_length_items + kMinPduLen) {
    LOG(INFO) << "PDU too short: have " << pdu_len << " need "
              << variable_length_items + kMinPduLen;
    return NULL;
  }

  uint8_t tp_pid_offset = msg_start_offset + 3 + sender_addr_num_octets;
  uint8_t user_data_len_offset = tp_pid_offset + 2 + kSmscTimestampLen;
  uint8_t user_data_num_septets = pdu[user_data_len_offset];
  variable_length_items += (7 * (user_data_num_septets + 1 )) / 8;

  if (pdu_len < variable_length_items + kMinPduLen) {
    LOG(INFO) << "PDU too short: have " << pdu_len << " need "
              << variable_length_items + kMinPduLen;
    return NULL;
  }

  // smsc number format must be international, E.164
  if (pdu[1] != kTypeOfAddrIntlE164) {
    LOG(INFO) << "Invalid SMSC address format: have "
              << std::hex << (int)pdu[1] << " need "
              << std::hex << kTypeOfAddrIntlE164;
    return NULL;
  }

  // we only handle SMS-DELIVER messages
  if ((pdu[msg_start_offset] & kMsgTypeMask) != kMsgTypeDeliver) {
    LOG(INFO) << "Unhandled message type: have "
              << std::hex << (int)(pdu[msg_start_offset] & kMsgTypeMask)
              << " need " << std::hex << kMsgTypeDeliver;
    return NULL;
  }

  uint8_t sender_addr_type = pdu[msg_start_offset + 2];

  uint8_t dcs = pdu[tp_pid_offset+1];
  enum {dcs_unknown, dcs_gsm7, dcs_ucs2, dcs_8bit} scheme = dcs_unknown;

  switch ((dcs >> 4) & 0xf) {
    // General data coding group
    case 0: case 1:
    case 2: case 3:
      switch (dcs & 0x0c) {
        case 0x8:
          scheme = dcs_ucs2;
          break;
        case 0:
        case 0xc:    // reserved - spec says to treat it as default alphabet
          scheme = dcs_gsm7;
          break;
        case 0x4:
          scheme = dcs_8bit;
          break;
      }
      break;

    // Message waiting group (default alphabet)
    case 0xc:
    case 0xd:
      scheme = dcs_gsm7;
      break;

    // Message waiting group (UCS2 alphabet)
    case 0xe:
      scheme = dcs_ucs2;
      break;

    // Data coding/message class group
    case 0xf:
      switch (dcs & 0x04) {
        case 0:
          scheme = dcs_gsm7;
          break;
        case 0x4:
          scheme = dcs_8bit;
          break;
      }
      break;

    // Reserved coding group values - spec says to treat it as default alphabet
    default:
      scheme = dcs_gsm7;
      break;
  }

  if (scheme == dcs_unknown) {
    LOG(INFO) << "Unhandled data coding scheme: " << std::hex << (int)dcs;
    return NULL;
  }

  std::string smsc_addr = "+" + SemiOctetsToBcdString(&pdu[2],
                                                      smsc_addr_num_octets-1);
  std::string sender_addr;
  if ((sender_addr_type & kTypeOfAddrNumMask) != kTypeOfAddrNumAlpha) {
    sender_addr = SemiOctetsToBcdString(&pdu[msg_start_offset+3],
                                        sender_addr_num_octets);
    if ((sender_addr_type & kTypeOfAddrNumMask) == kTypeOfAddrNumIntl) {
      sender_addr = "+" + sender_addr;
    }
  } else {
    size_t datalen = (sender_addr_num_digits * 4) / 7;
    sender_addr = utilities::Gsm7ToUtf8String(&pdu[msg_start_offset+3],
                                              datalen, 0);
  }

  std::string msg_text;
  size_t num_data_octets;
  size_t data_octets_available;

  data_octets_available = pdu_len - (user_data_len_offset + 1);

  if (scheme == dcs_gsm7)
    num_data_octets = (pdu[user_data_len_offset] * 7 + 7)/8;
  else if (scheme == dcs_ucs2)
    num_data_octets = pdu[user_data_len_offset];
  else
    num_data_octets = pdu[user_data_len_offset];

  if (data_octets_available < num_data_octets) {
    LOG(INFO) << "Specified user data length (" << num_data_octets
              << ") exceeds data available";
    return NULL;
  }

  uint8_t user_data_header_len = 0;
  uint8_t msg_body_offset = user_data_len_offset + 1;
  uint8_t user_data_offset = user_data_len_offset+1;
  size_t user_data_len = pdu[user_data_len_offset];
  uint8_t bit_offset = 0;

  uint16_t part_reference = 0;
  uint8_t part_sequence = 1;
  uint8_t part_count = 1;

  if ((pdu[msg_start_offset] & kTpUdhi) == kTpUdhi)
    user_data_header_len = pdu[user_data_offset] + 1;

  if (user_data_header_len != 0) {
    // Parse the user data headers
    for (int offset = msg_body_offset + 1;
         offset < msg_body_offset + user_data_header_len; ) {
      // Information element ID and length
      int ie_id = pdu[offset++];
      int ie_len = pdu[offset++];
      LOG(INFO) << "Information element type " << ie_id
                << " len " << ie_len;
      if (ie_id == kConcatenatedSms8bit && ie_len == 3) {
        part_reference = pdu[offset];
        part_count = pdu[offset + 1];
        part_sequence = pdu[offset + 2];
        LOG(INFO) << "ref " << (int)part_reference
                  << " count " << (int)part_count
                  << " seq " << (int)part_sequence;
      } else if (ie_id == kConcatenatedSms16bit && ie_len == 4) {
        part_reference = (pdu[offset] << 8) | pdu[offset + 1];
        part_count = pdu[offset + 2];
        part_sequence = pdu[offset + 3];
        LOG(INFO) << "ref " << (int)part_reference
                  << " count " << (int)part_count
                  << " seq " << (int)part_sequence;
      }
      offset += ie_len;
    }
    msg_body_offset += user_data_header_len;
  }

  if (scheme == dcs_gsm7) {
    if (user_data_header_len != 0) {
      size_t len_adjust = (user_data_header_len * 8 + 6) / 7;
      user_data_len -= len_adjust;
      bit_offset = len_adjust * 7 - user_data_header_len * 8;
    }
    msg_text = utilities::Gsm7ToUtf8String(&pdu[msg_body_offset],
                                           user_data_len,
                                           bit_offset);
  } else if (scheme == dcs_ucs2) {
    user_data_len -= user_data_header_len;
    msg_text = utilities::Ucs2ToUtf8String(&pdu[msg_body_offset],
                                           user_data_len);
  } else {  // 8-bit data: just copy it as-is
    user_data_len -= user_data_header_len;
    msg_text.assign(reinterpret_cast<const char *>(&pdu[msg_body_offset]),
                    user_data_len);
  }

  std::string sc_timestamp = SemiOctetsToBcdString(&pdu[tp_pid_offset+2],
                                                   kSmscTimestampLen-1);
  // The last two semi-octets of the timestamp indicate an offset from
  // GMT, where bit 3 of the first semi-octet is interpreted as a sign bit
  uint8_t tzoff_octet = pdu[tp_pid_offset+1+kSmscTimestampLen];
  char sign = (tzoff_octet & 0x8) ? '-' : '+';
  sc_timestamp += sign;
  int quarter_hours = (tzoff_octet & 0x7) * 10 + ((tzoff_octet >> 4) & 0xf);
  sc_timestamp += (quarter_hours / 40) + '0';
  sc_timestamp += (quarter_hours / 4) % 10 + '0';

  return new SmsMessageFragment(smsc_addr, sender_addr, sc_timestamp, msg_text,
                                part_reference, part_sequence, part_count,
                                index);
}

SmsMessage::SmsMessage(SmsMessageFragment *base) :
    base_(base),
    num_remaining_parts_(base->part_count() - 1),
    fragments_(base->part_count())
{
  fragments_[base->part_sequence() - 1] = base;
  LOG(INFO) << "Created new message with base ref " << base->part_reference();
}

void SmsMessage::add(SmsMessageFragment *sms)
{
  if (sms->part_reference() != base_->part_reference()) {
    LOG(WARNING) << "Attempt to add SMS part with reference "
                 << sms->part_reference()
                 << " to multipart SMS with reference "
                 << base_->part_reference();
    return;
  }
  int sequence = sms->part_sequence();
  if (sequence > base_->part_count()) {
    LOG(WARNING) << "SMS part out of range: "
                 << sequence << " vs. " << base_->part_count();
    return;
  }
  if (fragments_[sequence - 1] != NULL) {
    LOG(WARNING) << "Part " << sequence << " already exists in message";
    return;
  }
  num_remaining_parts_--;
  fragments_[sequence - 1] = sms;
}

bool SmsMessage::is_complete()
{
  return num_remaining_parts_ == 0;
}

std::string& SmsMessage::text()
{
  // This could be more clever about not recomputing if the message is complete.
  composite_text_.clear();

  for (int i = 0 ; i < base_->part_count(); i++)
    if (fragments_[i] != NULL)
      composite_text_ += fragments_[i]->text();

  return composite_text_;
}

std::vector<int> SmsMessage::index_list()
{
  std::vector<int> ret;
  for (std::vector<SmsMessageFragment *>::iterator it = fragments_.begin();
       it != fragments_.end();
       ++it) {
    if (*it != NULL)
      ret.push_back((*it)->index());
  }

  return ret;
}

SmsMessage::~SmsMessage()
{
  for (std::vector<SmsMessageFragment *>::iterator it = fragments_.begin();
       it != fragments_.end();
       ++it) {
    if (*it != NULL) {
      delete *it;
      *it = NULL;
    }
  }
}
