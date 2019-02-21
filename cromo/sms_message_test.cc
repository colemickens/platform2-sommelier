// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Unit tests for SMS message creation

#include "cromo/sms_message.h"

#include <base/logging.h>
#include <gtest/gtest.h>

// TODO(ers) Add more negative test cases

namespace cromo {
namespace {

TEST(CreateSmsMessage, SimpleMessage) {
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,  // SMSC address
    0x04,       // SMS-DELIVER
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x81, 0x00, 0x55, 0x15, 0x12, 0xf2,  // sender address
    0x00,       // TP-PID protocol identifier
    0x00,       // TP-DCS data coding scheme
    0x11, 0x10, 0x10, 0x21, 0x43, 0x65, 0x00,  // TP-SCTS timestamp
    0x0a,       // TTP-UDL user data length
    // TP-UD user data:
    0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
  };
  SmsMessageFragment* frag = SmsMessageFragment::CreateFragment(kPdu,
                                                                sizeof(kPdu),
                                                                1);

  ASSERT_NE(nullptr, frag);
  EXPECT_EQ("+12345678901", frag->smsc_address());
  EXPECT_EQ("+18005551212", frag->sender_address());
  EXPECT_EQ("110101123456+00", frag->timestamp());
  EXPECT_EQ("hellohello", frag->text());
  EXPECT_EQ(1, frag->index());
  EXPECT_EQ(1, frag->part_count());
  EXPECT_EQ(1, frag->part_sequence());

  SmsMessage* sms = new SmsMessage(frag);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456+00", sms->timestamp());
  EXPECT_EQ("hellohello", sms->GetMessageText());
  EXPECT_EQ(1, sms->index());
  EXPECT_TRUE(sms->IsComplete());
}

TEST(CreateSmsMessage, ExtendedChars) {
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x04, 0x44, 0x29, 0x61, 0xf4,  // SMSC address
    0x04,       // SMS-DELIVER
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x61, 0x71, 0x95, 0x72, 0x91, 0xf8,  // sender address
    0x00,       // TP-PID protocol identifier
    0x00,       // TP-DCS data coding scheme
    0x11, 0x20, 0x82, 0x11, 0x05, 0x05, 0x0a,  // TP-SCTS timestamp

    0x6a,       // TTP-UDL user data length
    // TP-UD user data:
    0xc8, 0xb2, 0xbc, 0x7c, 0x9a, 0x83, 0xc2,
    0x20, 0xf6, 0xdb, 0x7d, 0x2e, 0xcb, 0x41, 0xed,
    0xf2, 0x7c, 0x1e, 0x3e, 0x97, 0x41, 0x1b, 0xde,
    0x06, 0x75, 0x4f, 0xd3, 0xd1, 0xa0, 0xf9, 0xbb,
    0x5d, 0x06, 0x95, 0xf1, 0xf4, 0xb2, 0x9b, 0x5c,
    0x26, 0x83, 0xc6, 0xe8, 0xb0, 0x3c, 0x3c, 0xa6,
    0x97, 0xe5, 0xf3, 0x4d, 0x6a, 0xe3, 0x03, 0xd1,
    0xd1, 0xf2, 0xf7, 0xdd, 0x0d, 0x4a, 0xbb, 0x59,
    0xa0, 0x79, 0x7d, 0x8c, 0x06, 0x85, 0xe7, 0xa0,
    0x00, 0x28, 0xec, 0x26, 0x83, 0x2a, 0x96, 0x0b,
    0x28, 0xec, 0x26, 0x83, 0xbe, 0x60, 0x50, 0x78,
    0x0e, 0xba, 0x97, 0xd9, 0x6c, 0x17
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12404492164", sms->smsc_address());
  EXPECT_EQ("+16175927198", sms->sender_address());
  EXPECT_EQ("110228115050-05", sms->timestamp());
  EXPECT_EQ("Here's a longer message [{with some extended characters}] "
            "thrown in, such as £ and ΩΠΨ and §¿ as well.", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, AlphaSenderAndUcs2Text) {
  static const uint8_t kPdu[] = {
    0x07, 0x91, 0x97, 0x30, 0x07, 0x11, 0x11, 0xf1,
    0x04, 0x14, 0xd0, 0x49, 0x37, 0xbd, 0x2c, 0x77,
    0x97, 0xe9, 0xd3, 0xe6, 0x14, 0x00, 0x08, 0x11,
    0x30, 0x92, 0x91, 0x02, 0x40, 0x61, 0x08, 0x04,
    0x42, 0x04, 0x35, 0x04, 0x41, 0x04, 0x42
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+79037011111", sms->smsc_address());
  EXPECT_EQ("InternetSMS", sms->sender_address());
  EXPECT_EQ("110329192004+04", sms->timestamp());
  EXPECT_EQ("тест", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, NonzeroPid) {
  // pid is nonzero (00 -> ff)
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,  // SMSC address
    0x04,       // SMS-DELIVER
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x81, 0x00, 0x55, 0x15, 0x12, 0xf2,  // sender address
    0xff,       // TP-PID protocol identifier
    0x00,       // TP-DCS data coding scheme
    0x11, 0x10, 0x10, 0x21, 0x43, 0x65, 0x00,  // TP-SCTS timestamp
    0x0a,       // TTP-UDL user data length
    // TP-UD user data:
    0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456+00", sms->timestamp());
  EXPECT_EQ("hellohello", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, MoreMessagesBitClear) {
  // TP-MMS is clear (04 -> 00)
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,  // SMSC address
    0x00,       // SMS-DELIVER (TP-MMS is clear)
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x81, 0x00, 0x55, 0x15, 0x12, 0xf2,  // sender address
    0xff,       // TP-PID protocol identifier
    0x00,       // TP-DCS data coding scheme
    0x11, 0x10, 0x10, 0x21, 0x43, 0x65, 0x00,  // TP-SCTS timestamp
    0x0a,       // TTP-UDL user data length
    // TP-UD user data:
    0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456+00", sms->timestamp());
  EXPECT_EQ("hellohello", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, TimeZoneOffsetGreaterThanTen) {
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,  // SMSC address
    0x04,       // SMS-DELIVER
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x81, 0x00, 0x55, 0x15, 0x12, 0xf2,  // sender address
    0x00,       // TP-PID protocol identifier
    0x00,       // TP-DCS data coding scheme
    0x11, 0x10, 0x10, 0x21, 0x43, 0x65, 0x21,  // TP-SCTS timestamp
    0x0a,       // TTP-UDL user data length
    // TP-UD user data:
    0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
  };
  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456+03", sms->timestamp());
  EXPECT_EQ("hellohello", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, NegativeTimeZoneOffset) {
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,  // SMSC address
    0x04,       // SMS-DELIVER
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x81, 0x00, 0x55, 0x15, 0x12, 0xf2,  // sender address
    0x00,       // TP-PID protocol identifier
    0x00,       // TP-DCS data coding scheme
    0x11, 0x10, 0x10, 0x21, 0x43, 0x65, 0x29,  // TP-SCTS timestamp
    0x0a,       // TTP-UDL user data length
    // TP-UD user data:
    0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
  };
  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456-03", sms->timestamp());
  EXPECT_EQ("hellohello", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, NationalSenderNumber) {
  // number is natl (91 -> 81)
  static const uint8_t kPdu[] = {
    0x07, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,
    0x04, 0x0b, 0x81, 0x81, 0x00, 0x55, 0x15, 0x12,
    0xf2, 0x00, 0x00, 0x11, 0x10, 0x10, 0x21, 0x43,
    0x65, 0x00, 0x0a, 0xe8, 0x32, 0x9b, 0xfd, 0x46,
    0x97, 0xd9, 0xec, 0x37
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("18005551212", sms->sender_address());  // no plus
  EXPECT_EQ("110101123456+00", sms->timestamp());
  EXPECT_EQ("hellohello", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

#define CC(x) static_cast<char>(x)
static const char kExpected8bitData[] = {
  CC(0xe8), CC(0x32), CC(0x9b), CC(0xfd), CC(0x46),
  CC(0x97), CC(0xd9), CC(0xec), CC(0x37), CC(0xde),
  CC('\0')
};
#undef CC

TEST(CreateSmsMessage, 8BitData) {
  static const uint8_t kPdu[] = {
    0x07, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,
    0x04, 0x0b, 0x91, 0x81, 0x00, 0x55, 0x15, 0x12,
    0xf2, 0x00, 0x04, 0x11, 0x10, 0x10, 0x21, 0x43,
    0x65, 0x00, 0x0a, 0xe8, 0x32, 0x9b, 0xfd, 0x46,
    0x97, 0xd9, 0xec, 0x37, 0xde
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456+00", sms->timestamp());
  EXPECT_EQ(kExpected8bitData, sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, InsufficientUserData) {
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,  // SMSC address
    0x04,       // SMS-DELIVER
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x81, 0x00, 0x55, 0x15, 0x12, 0xf2,  // sender address
    0x00,       // TP-PID protocol identifier
    0x00,       // TP-DCS data coding scheme
    0x11, 0x10, 0x10, 0x21, 0x43, 0x65, 0x00,  // TP-SCTS timestamp
    0x0b,       // TTP-UDL user data length (too large)
    // TP-UD user data:
    0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  EXPECT_EQ(nullptr, sms);
}

TEST(CreateSmsMessage, GroupFDataCodingScheme) {
  static const uint8_t kPdu[] = {
    0x07,        // length of SMSC info
    0x91,        // type of address of SMSC (E.164)
    0x33, 0x06, 0x09, 0x10, 0x93, 0xF0,  // SMSC address (+33 60 90 01 39 0)
    0x04,        // SMS-DELIVER
    0x04,        // address length
    0x85,        // type of address
    0x81, 0x00,  // sender address (1800)
    0x00,        // TP-PID protocol identifier
    0xF1,        // TP-DCS data coding scheme
    0x11, 0x60, 0x42, 0x31, 0x80, 0x51, 0x80,   // timestamp 11-06-24 13:08:51
    0xA0,        // TP-UDL user data length (160)
    // Content:
    0x49,
    0xB7, 0xF9, 0x0D, 0x9A, 0x1A, 0xA5, 0xA0, 0x16,
    0x68, 0xF8, 0x76, 0x9B, 0xD3, 0xE4, 0xB2, 0x9B,
    0x9E, 0x2E, 0xB3, 0x59, 0xA0, 0x3F, 0xC8, 0x5D,
    0x06, 0xA9, 0xC3, 0xED, 0x70, 0x7A, 0x0E, 0xA2,
    0xCB, 0xC3, 0xEE, 0x79, 0xBB, 0x4C, 0xA7, 0xCB,
    0xCB, 0xA0, 0x56, 0x43, 0x61, 0x7D, 0xA7, 0xC7,
    0x69, 0x90, 0xFD, 0x4D, 0x97, 0x97, 0x41, 0xEE,
    0x77, 0xDD, 0x5E, 0x0E, 0xD7, 0x41, 0xED, 0x37,
    0x1D, 0x44, 0x2E, 0x83, 0xE0, 0xE1, 0xF9, 0xBC,
    0x0C, 0xD2, 0x81, 0xE6, 0x77, 0xD9, 0xB8, 0x4C,
    0x06, 0xC1, 0xDF, 0x75, 0x39, 0xE8, 0x5C, 0x90,
    0x97, 0xE5, 0x20, 0xFB, 0x9B, 0x2E, 0x2F, 0x83,
    0xC6, 0xEF, 0x36, 0x9C, 0x5E, 0x06, 0x4D, 0x8D,
    0x52, 0xD0, 0xBC, 0x2E, 0x07, 0xDD, 0xEF, 0x77,
    0xD7, 0xDC, 0x2C, 0x77, 0x99, 0xE5, 0xA0, 0x77,
    0x1D, 0x04, 0x0F, 0xCB, 0x41, 0xF4, 0x02, 0xBB,
    0x00, 0x47, 0xBF, 0xDD, 0x65, 0x50, 0xB8, 0x0E,
    0xCA, 0xD9, 0x66
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+33609001390", sms->smsc_address());
  EXPECT_EQ("1800", sms->sender_address());
  EXPECT_EQ("110624130815+02", sms->timestamp());
  EXPECT_EQ("Info SFR - Confidentiel, à ne jamais transmettre -\r\n"
            "Voici votre nouveau mot de passe : sw2ced pour gérer "
            "votre compte SFR sur www.sfr.fr ou par téléphone au 963",
            sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, GroupF8BitDataCodingScheme) {
  static const uint8_t kPdu[] = {
    0x07, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,
    0x04, 0x0b, 0x91, 0x81, 0x00, 0x55, 0x15, 0x12,
    0xf2, 0x00, 0xf4, 0x11, 0x10, 0x10, 0x21, 0x43,
    0x65, 0x00, 0x0a, 0xe8, 0x32, 0x9b, 0xfd, 0x46,
    0x97, 0xd9, 0xec, 0x37, 0xde
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456+00", sms->timestamp());
  EXPECT_EQ(kExpected8bitData, sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, ReservedCodingScheme) {
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,  // SMSC address
    0x04,       // SMS-DELIVER
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x81, 0x00, 0x55, 0x15, 0x12, 0xf2,  // sender address
    0x00,       // TP-PID protocol identifier
    0x0c,       // TP-DCS data coding scheme (reserved value)
    0x11, 0x10, 0x10, 0x21, 0x43, 0x65, 0x00,  // TP-SCTS timestamp
    0x0a,       // TTP-UDL user data length
    // TP-UD user data:
    0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
  };
  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);

  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456+00", sms->timestamp());
  EXPECT_EQ("hellohello", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, UserDataHeaderWithFillBits) {
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,  // SMSC address
    0x44,       // SMS-DELIVER (with TP-UDHI)
    0x0b,       // sender address length
    0x91,       // type of sender address
    0x81, 0x00, 0x55, 0x15, 0x12, 0xf2,  // sender address
    0x00,       // TP-PID protocol identifier
    0x00,       // TP-DCS data coding scheme
    0x11, 0x10, 0x10, 0x21, 0x43, 0x65, 0x00,  // TP-SCTS timestamp
    0x0f,       // TTP-UDL user data length
    // user data header:
    0x3,        // user data header length
    // a single user data header information element
    0x0, 0x1, 0x2,
    // TP-UD user data (first byte has 3 fill bits):
    0x40, 0x97, 0xd9, 0xec, 0x37, 0xba, 0xcc, 0x66, 0xbf, 0x01
  };
  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);

  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("+12345678901", sms->smsc_address());
  EXPECT_EQ("+18005551212", sms->sender_address());
  EXPECT_EQ("110101123456+00", sms->timestamp());
  EXPECT_EQ("hellohello", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

TEST(CreateSmsMessage, UserDataHeaderNoFillBits) {
  static const uint8_t kPdu[] = {
    0x07,       // length of SMSC info
    0x91,       // type of address of SMSC (E.164)
    0x13, 0x56, 0x13, 0x13, 0x13, 0xf6,
    0x40,         // SMS-DELIVER with TP-UDHI + TP-MMI
    0x04,         // sender address length
    0x85,         // type of sender address
    0x01, 0x20,   // sender address
    0x39,         // TP-PID
    0x00,         // TP-DCS
    0x11, 0x60, 0x92, 0x32, 0x23, 0x91, 0x80,     // TP-SCTS
    0xa0,         // TP-UDL (160 septets)
    0x06,         // user data header length
    0x08,         // info element ID
    0x04,         // info element data length
    0x00, 0x10, 0x02, 0x01,  // info element data
    // content:
    0xd7, 0x32,
    0x7b, 0xfd, 0x6e, 0xb3, 0x40, 0xe2, 0x32, 0x1b,
    0xf4, 0x6e, 0x83, 0xea, 0x77, 0x90, 0xf5, 0x9d,
    0x1e, 0x97, 0xdb, 0xe1, 0x34, 0x1b, 0x44, 0x2f,
    0x83, 0xc4, 0x65, 0x76, 0x3d, 0x3d, 0xa7, 0x97,
    0xe5, 0x65, 0x37, 0xc8, 0x1d, 0x0e, 0xcb, 0x41,
    0xab, 0x59, 0xcc, 0x16, 0x93, 0xc1, 0x60, 0x31,
    0xd9, 0x6c, 0x06, 0x42, 0x41, 0xe5, 0x65, 0x68,
    0x38, 0xaf, 0x03, 0xa9, 0x62, 0x30, 0x98, 0x2a,
    0x26, 0x9b, 0xcd, 0x46, 0x29, 0x17, 0xc8, 0xfa,
    0x4e, 0x8f, 0xcb, 0xed, 0x70, 0x9a, 0x0d, 0x7a,
    0xbb, 0xe9, 0xf6, 0xb0, 0xfb, 0x5c, 0x76, 0x83,
    0xd2, 0x73, 0x50, 0x98, 0x4d, 0x4f, 0xab, 0xc9,
    0xa0, 0xb3, 0x3c, 0x4c, 0x4f, 0xcf, 0x5d, 0x20,
    0xeb, 0xfb, 0x2d, 0x07, 0x9d, 0xcb, 0x62, 0x79,
    0x3d, 0xbd, 0x06, 0xd9, 0xc3, 0x6e, 0x50, 0xfb,
    0x2d, 0x4e, 0x97, 0xd9, 0xa0, 0xb4, 0x9b, 0x5e,
    0x96, 0xbb, 0xcb
  };
  SmsMessageFragment* frag = SmsMessageFragment::CreateFragment(kPdu,
                                                                sizeof(kPdu),
                                                                1);

  ASSERT_NE(nullptr, frag);
  EXPECT_EQ("+31653131316", frag->smsc_address());
  EXPECT_EQ("1002", frag->sender_address());
  EXPECT_EQ("110629233219+02", frag->timestamp());
  EXPECT_EQ("Welkom, bel om uw Voicemail te beluisteren naar +31612001233"
            " (PrePay: *100*1233#). Voicemail ontvangen is altijd gratis."
            " Voor gebruik van mobiel interne", frag->text());
  EXPECT_EQ(1, frag->index());
  EXPECT_EQ(0x0010, frag->part_reference());
  EXPECT_EQ(2, frag->part_count());
  EXPECT_EQ(1, frag->part_sequence());

  SmsMessage* sms = new SmsMessage(frag);
  EXPECT_EQ("+31653131316", sms->smsc_address());
  EXPECT_EQ("1002", sms->sender_address());
  EXPECT_EQ("110629233219+02", sms->timestamp());
  EXPECT_EQ("Welkom, bel om uw Voicemail te beluisteren naar +31612001233"
            " (PrePay: *100*1233#). Voicemail ontvangen is altijd gratis."
            " Voor gebruik van mobiel interne", sms->GetMessageText());
  EXPECT_FALSE(sms->IsComplete());
  EXPECT_EQ(0x0010, sms->part_reference());
}

TEST(CreateSmsMessage, TwoPart) {
  static const uint8_t kPdu1[] = {
    0x07, 0x91, 0x41, 0x40, 0x54, 0x05, 0x10, 0xf0,
    0x44, 0x0b, 0x91, 0x61, 0x71, 0x05, 0x64, 0x29,
    0xf5, 0x00, 0x00, 0x11, 0x01, 0x52, 0x41, 0x04,
    0x41, 0x8a, 0xa0, 0x05, 0x00, 0x03, 0x9c, 0x02,
    0x01, 0xa8, 0xe8, 0xf4, 0x1c, 0x94, 0x9e, 0x83,
    0xc2, 0x20, 0x7a, 0x79, 0x4e, 0x77, 0x29, 0x82,
    0xa0, 0x3b, 0x3a, 0x4c, 0xff, 0x81, 0x82, 0x20,
    0x7a, 0x79, 0x4e, 0x77, 0x81, 0xa8, 0xe8, 0xf4,
    0x1c, 0x94, 0x9e, 0x83, 0xde, 0x6e, 0x76, 0x1e,
    0x14, 0x06, 0xd1, 0xcb, 0x73, 0xba, 0x4b, 0x01,
    0xa2, 0xa2, 0xd3, 0x73, 0x50, 0x7a, 0x0e, 0x0a,
    0x83, 0xe8, 0xe5, 0x39, 0xdd, 0xa5, 0x08, 0x82,
    0xee, 0xe8, 0x30, 0xfd, 0x07, 0x0a, 0x82, 0xe8,
    0xe5, 0x39, 0xdd, 0x05, 0xa2, 0xa2, 0xd3, 0x73,
    0x50, 0x7a, 0x0e, 0x7a, 0xbb, 0xd9, 0x79, 0x50,
    0x18, 0x44, 0x2f, 0xcf, 0xe9, 0x2e, 0x05, 0x88,
    0x8a, 0x4e, 0xcf, 0x41, 0xe9, 0x39, 0x28, 0x0c,
    0xa2, 0x97, 0xe7, 0x74, 0x97, 0x22, 0x08, 0xba,
    0xa3, 0xc3, 0xf4, 0x1f, 0x28, 0x08, 0xa2, 0x97,
    0xe7, 0x74, 0x17, 0x88, 0x8a, 0x4e, 0xcf, 0x41,
    0xe9, 0x39, 0xe8, 0xed, 0x66, 0xe7, 0x41
  };

  static const uint8_t kPdu2[] = {
    0x07, 0x91, 0x41, 0x40, 0x54, 0x05, 0x10, 0xf1,
    0x44, 0x0b, 0x91, 0x61, 0x71, 0x05, 0x64, 0x29,
    0xf5, 0x00, 0x00, 0x11, 0x01, 0x52, 0x41, 0x04,
    0x51, 0x8a, 0x1d, 0x05, 0x00, 0x03, 0x9c, 0x02,
    0x02, 0xc2, 0x20, 0x7a, 0x79, 0x4e, 0x77, 0x81,
    0xa6, 0xe5, 0xf1, 0xdb, 0x4d, 0x06, 0xb5, 0xcb,
    0xf3, 0x79, 0xf8, 0x5c, 0x06,
  };

  SmsMessageFragment* frag1 = SmsMessageFragment::CreateFragment(kPdu1,
                                                                 sizeof(kPdu1),
                                                                 1);

  ASSERT_NE(nullptr, frag1);
  EXPECT_EQ("+14044550010", frag1->smsc_address());
  EXPECT_EQ("+16175046925", frag1->sender_address());
  EXPECT_EQ("111025144014-07", frag1->timestamp());
  const char frag1_text[] =
      "This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only ";
  EXPECT_EQ(frag1_text, frag1->text());
  EXPECT_EQ(1, frag1->index());
  EXPECT_EQ(156, frag1->part_reference());
  EXPECT_EQ(2, frag1->part_count());
  EXPECT_EQ(1, frag1->part_sequence());

  SmsMessageFragment* frag2 = SmsMessageFragment::CreateFragment(kPdu2,
                                                                 sizeof(kPdu2),
                                                                 2);
  ASSERT_NE(nullptr, frag2);
  EXPECT_EQ("+14044550011", frag2->smsc_address());
  EXPECT_EQ("+16175046925", frag2->sender_address());
  EXPECT_EQ("111025144015-07", frag2->timestamp());
  EXPECT_EQ("a test. Second message", frag2->text());
  EXPECT_EQ(2, frag2->index());
  EXPECT_EQ(156, frag2->part_reference());
  EXPECT_EQ(2, frag2->part_count());
  EXPECT_EQ(2, frag2->part_sequence());

  SmsMessage* sms = new SmsMessage(frag1);

  ASSERT_NE(nullptr, sms);
  EXPECT_FALSE(sms->IsComplete());

  sms->AddFragment(frag2);
  EXPECT_EQ("+14044550010", sms->smsc_address());
  EXPECT_EQ("+16175046925", sms->sender_address());
  EXPECT_EQ("111025144014-07", sms->timestamp());
  const char sms_text[] =
      "This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test. Second message";
  EXPECT_EQ(sms_text, sms->GetMessageText());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(156, sms->part_reference());
  EXPECT_TRUE(sms->IsComplete());

  SmsMessage* sms2 = new SmsMessage(frag2);
  ASSERT_NE(nullptr, sms2);
  EXPECT_FALSE(sms2->IsComplete());

  sms2->AddFragment(frag1);
  EXPECT_EQ("+14044550011", sms2->smsc_address());
  EXPECT_EQ("+16175046925", sms2->sender_address());
  EXPECT_EQ("111025144015-07", sms2->timestamp());
  EXPECT_EQ(sms_text, sms2->GetMessageText());
  EXPECT_EQ(2, sms2->index());
  EXPECT_EQ(156, sms2->part_reference());
  EXPECT_TRUE(sms2->IsComplete());
}


TEST(CreateSmsMessage, NonIntlSMSC) {
  static const uint8_t kPdu[] = {
    0x03,        // length of SMSC info
    0x80,        // type of address of SMSC (unknown)
    0x98, 0x06,  // SMSC address
    0x04,        // SMS-DELIVER
    0x04,        // sender address length
    0x80,        // type of sender address (unknown)
    0x98, 0x06,  // sender address
    0x00,        // TP-PID protocol identifier
    0xf0,        // TP-DCS data coding scheme
    0x21, 0x20, 0x11, 0x12, 0x74, 0x12, 0x00,  // TP-SCTS timestamp
    0x0a,        // TP-UDL user data length
    // TP-UD user data:
    0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
  };

  SmsMessageFragment* sms = SmsMessageFragment::CreateFragment(kPdu,
                                                               sizeof(kPdu),
                                                               1);
  ASSERT_NE(nullptr, sms);
  EXPECT_EQ("8960", sms->smsc_address());
  EXPECT_EQ("8960", sms->sender_address());
  EXPECT_EQ("120211214721+00", sms->timestamp());
  EXPECT_EQ("hellohello", sms->text());
  EXPECT_EQ(1, sms->index());
  EXPECT_EQ(1, sms->part_count());
}

}  // namespace
}  // namespace cromo
