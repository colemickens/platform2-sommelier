// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo/sms_message.h"

#include <base/logging.h>
#include <base/memory/ptr_util.h>

#include "cromo/utilities.h"

namespace cromo {

static const uint8_t kMsgTypeMask          = 0x03;
static const uint8_t kMsgTypeDeliver       = 0x00;
// udhi is "User Data Header Indicator"
static const uint8_t kTpUdhi               = 0x40;

static const uint8_t kTypeOfAddrNumMask    = 0x70;
static const uint8_t kTypeOfAddrNumIntl    = 0x10;
static const uint8_t kTypeOfAddrNumAlpha   = 0x50;

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
    const char first = NibbleToChar(octets[i] & 0xf);
    const char second = NibbleToChar((octets[i] >> 4) & 0xf);

    if (first != '\0')
      bcd += first;
    if (second != '\0')
      bcd += second;
  }
  return bcd;
}

static std::string DecodeAddress(const uint8_t* octets,
                                 uint8_t type,
                                 int num_octets) {
  std::string addr;

  if ((type & kTypeOfAddrNumMask) != kTypeOfAddrNumAlpha) {
    addr = SemiOctetsToBcdString(octets, num_octets);
    if ((type & kTypeOfAddrNumMask) == kTypeOfAddrNumIntl)
      addr = "+" + addr;
  } else {
    const size_t datalen = (num_octets * 8) / 7;
    addr = utilities::Gsm7ToUtf8String(octets, num_octets, datalen, 0);
  }
  return addr;
}

// Helper class to make it easy to extract successive bytes and byte
// ranges from a binary buffer.
class Bytes {
 public:
  Bytes(const uint8_t* pdu, int len) : pdu_(pdu), len_(len), offset_(0) {}

  // Return the number of bytes remaining to be consumed.
  int BytesLeft() const {
    return len_ - offset_;
  }

  // Return the next byte, or 0 if the buffer has been consumed.
  // Advances the internal pointer by one.
  uint8_t NextByte() {
    if (BytesLeft() >= 1)
      return pdu_[offset_++];
    else
      return 0;
  }

  // Return a pointer to the next N bytes, or nullptr if there aren't that many.
  // Advances the internal pointer by N if successful.
  const uint8_t* NextBytes(int n) {
    if (BytesLeft() >= n) {
      const uint8_t* bytes = pdu_ + offset_;
      offset_ += n;
      return bytes;
    } else {
      return nullptr;
    }
  }

 private:
  // Buffer of bytes.
  const uint8_t* pdu_;
  // Length of the buffer.
  const int len_;
  // Current position in the buffer.
  int offset_;
};

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

static bool parse_smsc_address(Bytes* bytes, std::string* smsc_address) {
  if (bytes->BytesLeft() < 2) {
    LOG(ERROR) << "PDU truncated in SMSC address header";
    return false;
  }
  const int smsc_addr_num_octets = bytes->NextByte() - 1;
  const int smsc_addr_type = bytes->NextByte();
  if (bytes->BytesLeft() < smsc_addr_num_octets) {
    LOG(ERROR) << "PDU truncated in SMSC address";
    return false;
  }
  *smsc_address = DecodeAddress(bytes->NextBytes(smsc_addr_num_octets),
                                smsc_addr_type,
                                smsc_addr_num_octets);
  return true;
}

static bool parse_sender_address(Bytes* bytes, std::string* sender_address) {
  if (bytes->BytesLeft() < 2) {
    LOG(ERROR) << "PDU truncated in sender address header";
    return false;
  }
  const int sender_addr_num_digits = bytes->NextByte();
  // round the sender address length up to an even number of semi-octets,
  // and thus an integral number of octets
  const int sender_addr_num_octets = (sender_addr_num_digits + 1) >> 1;
  const int sender_addr_type = bytes->NextByte();
  if (bytes->BytesLeft() < sender_addr_num_octets) {
    LOG(ERROR) << "PDU truncated in sender address";
    return false;
  }
  *sender_address = DecodeAddress(bytes->NextBytes(sender_addr_num_octets),
                                  sender_addr_type,
                                  sender_addr_num_octets);
  return true;
}

static bool parse_timestamp(Bytes* bytes, std::string* timestamp) {
  if (bytes->BytesLeft() < kSmscTimestampLen) {
    LOG(ERROR) << "PDU truncated in timestamp";
    return false;
  }
  *timestamp = SemiOctetsToBcdString(bytes->NextBytes(kSmscTimestampLen - 1),
                                     kSmscTimestampLen - 1);
  // The last two semi-octets of the timestamp indicate an offset from
  // GMT, where bit 3 of the first semi-octet is interpreted as a sign bit
  const int tzoff_octet = bytes->NextByte();
  const char sign = (tzoff_octet & 0x8) ? '-' : '+';
  *timestamp += sign;
  const int quarter_hours = (tzoff_octet & 0x7) * 10 +
      ((tzoff_octet >> 4) & 0xf);
  *timestamp += (quarter_hours / 40) + '0';
  *timestamp += (quarter_hours / 4) % 10 + '0';

  return true;
}

static bool parse_user_data_header(Bytes* bytes,
                                   int flags,
                                   int user_data_len,
                                   int* user_data_header_len,
                                   int* part_reference,
                                   int* part_sequence,
                                   int* part_count) {
  *part_reference = 0;
  *part_sequence = 1;
  *part_count = 1;

  *user_data_header_len = 0;
  if ((flags & kTpUdhi) == 0)
    return true;

  *user_data_header_len = bytes->NextByte() + 1;  // Include the length itself
  if (*user_data_header_len == 0)
    return true;
  if (bytes->BytesLeft() < *user_data_header_len) {
    LOG(ERROR) << "PDU truncated in user data header";
    return false;
  }
  // The user data is made up of a number of information elements,
  // which are each composed of an ID octet, a length octet, and the data.
  // The length octet is the length of the data, not of the entire element.
  for (int ie_offset = 1;
       ie_offset < *user_data_header_len; ) {
    int ie_id = bytes->NextByte();
    int ie_len = bytes->NextByte();
    if (ie_id == kConcatenatedSms8bit && ie_len == 3) {
      *part_reference = bytes->NextByte();
      *part_count = bytes->NextByte();
      *part_sequence = bytes->NextByte();
    } else if (ie_id == kConcatenatedSms16bit && ie_len == 4) {
      *part_reference = bytes->NextByte() << 8;
      *part_reference |= bytes->NextByte();
      *part_count = bytes->NextByte();
      *part_sequence = bytes->NextByte();
    } else {
      // Unknown information elements are simply skipped.
      bytes->NextBytes(ie_len);
    }
    ie_offset += ie_len + 2;
  }
  return true;
}

static bool parse_text(Bytes* bytes,
                       int user_data_len,
                       int user_data_header_len,
                       int data_coding_scheme,
                       std::string* text) {
  enum {kDcsUnknown, kDcsGsm7, kDcsUcs2, kDcs8bit} scheme = kDcsUnknown;

  switch ((data_coding_scheme >> 4) & 0xf) {
    // General data coding group
    case 0: case 1:
    case 2: case 3:
      switch (data_coding_scheme & 0x0c) {
        case 0x8:
          scheme = kDcsUcs2;
          break;
        case 0:
        case 0xc:  // reserved - spec says to treat it as default alphabet
          scheme = kDcsGsm7;
          break;
        case 0x4:
          scheme = kDcs8bit;
          break;
      }
      break;

    // Message waiting group (default alphabet)
    case 0xc:
    case 0xd:
      scheme = kDcsGsm7;
      break;

    // Message waiting group (UCS2 alphabet)
    case 0xe:
      scheme = kDcsUcs2;
      break;

    // Data coding/message class group
    case 0xf:
      switch (data_coding_scheme & 0x04) {
        case 0:
          scheme = kDcsGsm7;
          break;
        case 0x4:
          scheme = kDcs8bit;
          break;
      }
      break;

    // Reserved coding group values - spec says to treat it as default alphabet
    default:
      scheme = kDcsGsm7;
      break;
  }

  if (scheme == kDcsUnknown) {
    LOG(WARNING) << "Unhandled data coding scheme: " << std::hex
                 << data_coding_scheme;
    return false;
  }

  if (scheme == kDcsGsm7) {
    int bit_offset = 0;
    if (user_data_header_len != 0) {
      const int len_adjust = (user_data_header_len * 8 + 6) / 7;
      user_data_len -= len_adjust;
      bit_offset = len_adjust * 7 - user_data_header_len * 8;
    }
    const int user_data_octets = (user_data_len * 7 + bit_offset + 7) / 8;
    if (bytes->BytesLeft() < user_data_octets) {
      LOG(ERROR) << "PDU truncated in message text - needed "
                 << user_data_octets << " bytes, had "
                 << bytes->BytesLeft();
      return false;
    }
    *text = utilities::Gsm7ToUtf8String(bytes->NextBytes(user_data_octets),
                                        user_data_octets,
                                        user_data_len,
                                        bit_offset);
  } else if (scheme == kDcsUcs2) {
    user_data_len -= user_data_header_len;
    if (bytes->BytesLeft() < user_data_len) {
      LOG(ERROR) << "PDU truncated in message text - needed "
                 << user_data_len << " bytes, had "
                 << bytes->BytesLeft();
      return false;
    }
    *text = utilities::Ucs2ToUtf8String(bytes->NextBytes(user_data_len),
                                        user_data_len);
  } else {  // 8-bit data: just copy it as-is
    user_data_len -= user_data_header_len;
    if (bytes->BytesLeft() < user_data_len) {
      LOG(ERROR) << "PDU truncated in message text - needed "
                 << user_data_len << " bytes, had "
                 << bytes->BytesLeft();
      return false;
    }
    text->assign(reinterpret_cast<const char*>(bytes->NextBytes(user_data_len)),
                 user_data_len);
  }
  return true;
}


SmsMessageFragment* SmsMessageFragment::CreateFragment(const uint8_t* pdu,
                                                       size_t pdu_len,
                                                       int index) {
  // Extra parentheses quiets 'unreachable-code' warnings.
  if ((false)) {  // Change to true for debug logging of PDU data
    std::string hexpdu;
    static const char kHexDigits[]="0123456789abcdef";
    for (unsigned int i = 0 ; i < pdu_len ; i++) {
      hexpdu += kHexDigits[pdu[i] >> 4];
      hexpdu += kHexDigits[pdu[i] & 0xf];
    }
    LOG(INFO) << "PDU: " << hexpdu;
  }

  if (pdu_len < kMinPduLen) {
    LOG(ERROR) << "PDU too short - needed at least " << kMinPduLen
               << " bytes, had " << pdu_len;
    return nullptr;
  }

  Bytes bytes(pdu, pdu_len);


  std::string smsc_address;
  if (!parse_smsc_address(&bytes, &smsc_address))
    return nullptr;

  const int flags = bytes.NextByte();
  // we only handle SMS-DELIVER messages
  if ((flags & kMsgTypeMask) != kMsgTypeDeliver) {
    LOG(WARNING) << "Unhandled message type: have "
                 << std::hex << static_cast<int>(flags & kMsgTypeMask)
                 << " need " << std::hex << static_cast<int>(kMsgTypeDeliver);
    return nullptr;
  }

  std::string sender_address;
  if (!parse_sender_address(&bytes, &sender_address))
    return nullptr;
  bytes.NextByte();  // skip over the protocol byte
  const int data_coding_scheme = bytes.NextByte();
  std::string sc_timestamp;
  if (!parse_timestamp(&bytes, &sc_timestamp))
      return nullptr;
  const int user_data_len = bytes.NextByte();
  int user_data_header_len;
  int part_reference, part_sequence, part_count;
  if (!parse_user_data_header(&bytes, flags, user_data_len,
                              &user_data_header_len,
                              &part_reference, &part_sequence, &part_count))
    return nullptr;

  std::string message_text;
  if (!parse_text(&bytes, user_data_len, user_data_header_len,
                  data_coding_scheme, &message_text))
    return nullptr;

  return new SmsMessageFragment(smsc_address,
                                sender_address,
                                sc_timestamp,
                                message_text,
                                part_reference, part_sequence, part_count,
                                index);
}

SmsMessage::SmsMessage(SmsMessageFragment* base)
    : base_(base),
      num_remaining_parts_(base->part_count() - 1),
      fragments_(base->part_count()) {
  fragments_[base->part_sequence() - 1] = base::WrapUnique(base);
  LOG(INFO) << "Created new message with base ref " << base->part_reference();
}

void SmsMessage::AddFragment(SmsMessageFragment* sms) {
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
  if (fragments_[sequence - 1]) {
    LOG(WARNING) << "Part " << sequence << " already exists in message";
    return;
  }
  num_remaining_parts_--;
  fragments_[sequence - 1] = base::WrapUnique(sms);
}

bool SmsMessage::IsComplete() const {
  return num_remaining_parts_ == 0;
}

const std::string& SmsMessage::GetMessageText() {
  // This could be more clever about not recomputing if the message is complete.
  composite_text_.clear();

  for (int i = 0; i < base_->part_count(); i++) {
    if (fragments_[i])
      composite_text_ += fragments_[i]->text();
  }

  return composite_text_;
}

std::vector<int>* SmsMessage::MessageIndexList() const {
  std::vector<int>* ret = new std::vector<int>();
  for (const auto& fragment : fragments_) {
    if (fragment)
      ret->push_back(fragment->index());
  }

  return ret;
}

SmsMessage::~SmsMessage() {}

}  // namespace cromo
