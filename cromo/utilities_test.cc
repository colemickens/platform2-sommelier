// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Unit tests for utilities for the cromo modem manager

#include "cromo/utilities.h"

#include <base/logging.h>
#include <gtest/gtest.h>

TEST(Utilities, ExtractString) {
  using utilities::ExtractString;
  using utilities::DBusPropertyMap;

  DBusPropertyMap m;

  m["string"].writer().append_string("string");
  m["int32"].writer().append_int32(1);

  DBus::Error e_string;
  EXPECT_STREQ("string", ExtractString(m, "string", nullptr, e_string));
  EXPECT_FALSE(e_string.is_set());

  DBus::Error e_int32;
  EXPECT_EQ(nullptr, ExtractString(m, "int32", nullptr, e_int32));
  EXPECT_TRUE(e_int32.is_set());

  DBus::Error e_missing;
  EXPECT_STREQ("default",
               ExtractString(m, "not present", "default", e_missing));
  EXPECT_FALSE(e_missing.is_set());

  DBus::Error e_repeated_errors;
  EXPECT_STREQ("default",
               ExtractString(m, "int32", "default", e_repeated_errors));
  EXPECT_TRUE(e_repeated_errors.is_set());
  EXPECT_STREQ("default",
               ExtractString(m, "int32", "default", e_repeated_errors));
  EXPECT_TRUE(e_repeated_errors.is_set());
}

TEST(Utilities, HexEsnToDecimal) {
  using utilities::HexEsnToDecimal;

  std::string out;
  EXPECT_TRUE(HexEsnToDecimal("ffffffff", &out));
  EXPECT_STREQ("25516777215", out.c_str());

  EXPECT_TRUE(HexEsnToDecimal("80abcdef", &out));
  EXPECT_STREQ("12811259375", out.c_str());

  EXPECT_TRUE(HexEsnToDecimal("80000001", &out));
  EXPECT_STREQ("12800000001", out.c_str());

  EXPECT_TRUE(HexEsnToDecimal("1", &out));
  EXPECT_STREQ("00000000001", out.c_str());

  EXPECT_FALSE(HexEsnToDecimal("000bogus", &out));
  EXPECT_FALSE(HexEsnToDecimal("ffffffff" "f", &out));
}

const uint8_t gsm1[] = {
  10, 0xe8, 0x32, 0x9b, 0xfd, 0x46, 0x97, 0xd9, 0xec, 0x37
};
const uint8_t gsm2[] = {
  9, 0xd4, 0xf2, 0x9c, 0x0e, 0x9a, 0x36, 0xa7, 0x2e
};
const uint8_t gsm3[] = {
  10, 0xc9, 0x53, 0x1b, 0x24, 0x40, 0xf3, 0xdb, 0x65, 0x17
};
const uint8_t gsm4[] = { 2, 0x1b, 0x1e };
const uint8_t gsm5[] = {
  0x6a, 0xc8, 0xb2, 0xbc, 0x7c, 0x9a, 0x83, 0xc2,
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
const uint8_t gsm7_alphabet[] = {
  0x7f, 0x80, 0x80, 0x60, 0x40, 0x28, 0x18, 0x0e,
  0x88, 0x84, 0x62, 0xc1, 0x68, 0x38, 0x1e, 0x90,
  0x88, 0x64, 0x42, 0xa9, 0x58, 0x2e, 0x98, 0x8c,
  0x86, 0xd3, 0xf1, 0x7c, 0x40, 0x21, 0xd1, 0x88,
  0x54, 0x32, 0x9d, 0x50, 0x29, 0xd5, 0x8a, 0xd5,
  0x72, 0xbd, 0x60, 0x31, 0xd9, 0x8c, 0x56, 0xb3,
  0xdd, 0x70, 0x39, 0xdd, 0x8e, 0xd7, 0xf3, 0xfd,
  0x80, 0x41, 0xe1, 0x90, 0x58, 0x34, 0x1e, 0x91,
  0x49, 0xe5, 0x92, 0xd9, 0x74, 0x3e, 0xa1, 0x51,
  0xe9, 0x94, 0x5a, 0xb5, 0x5e, 0xb1, 0x59, 0xed,
  0x96, 0xdb, 0xf5, 0x7e, 0xc1, 0x61, 0xf1, 0x98,
  0x5c, 0x36, 0x9f, 0xd1, 0x69, 0xf5, 0x9a, 0xdd,
  0x76, 0xbf, 0xe1, 0x71, 0xf9, 0x9c, 0x5e, 0xb7,
  0xdf, 0xf1, 0x79, 0xfd, 0x9e, 0xdf, 0xf7, 0xff,
  0x01
};
const uint8_t gsm7_extended_chars[] = {
  0x14, 0x1b, 0xc5, 0x86, 0xb2, 0x41, 0x6d, 0x52,
  0x9b, 0xd7, 0x86, 0xb7, 0xe9, 0x6d, 0x7c, 0x1b,
  0xe0, 0xa6, 0x0c
};

static const struct {
  const std::string utf8_string;
  const uint8_t* packed_gsm7;
  size_t packed_gsm7_size;
} gsm7_test_data[] = {
  { "hellohello", gsm1, sizeof(gsm1) },
  { "Test SMS.", gsm2, sizeof(gsm2) },
  { "I'm $höme.", gsm3, sizeof(gsm3) },
  { "[", gsm4, sizeof(gsm4) },
  { "Here's a longer message [{with some extended characters}] thrown "
    "in, such as £ and ΩΠΨ and §¿ as well.", gsm5, sizeof(gsm5) },
  { "@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞÆæßÉ !\"#¤%&'()*+,-./"
    "0123456789:;<=>?¡ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "ÄÖÑÜ§¿abcdefghijklmnopqrstuvwxyzäöñüà",
    gsm7_alphabet,
    sizeof(gsm7_alphabet) },
  { "\xC^{}\\[~]|€", gsm7_extended_chars, sizeof(gsm7_extended_chars) },
  { "", nullptr }
};

TEST(Utilities, Gsm7ToUtf8) {
  using utilities::Gsm7ToUtf8String;
  std::string out;

  for (int i = 0; gsm7_test_data[i].packed_gsm7 != nullptr; ++i) {
    out = Gsm7ToUtf8String(&gsm7_test_data[i].packed_gsm7[1],
                           gsm7_test_data[i].packed_gsm7_size - 1,
                           gsm7_test_data[i].packed_gsm7[0],
                           0);
    EXPECT_EQ(gsm7_test_data[i].utf8_string, out);
  }
}

TEST(Utilities, Utf8ToGsm7) {
  using utilities::Utf8StringToGsm7;
  std::vector<uint8_t> out;

  for (int i = 0; gsm7_test_data[i].packed_gsm7 != nullptr; ++i) {
    out = Utf8StringToGsm7(gsm7_test_data[i].utf8_string);
    EXPECT_EQ(gsm7_test_data[i].packed_gsm7_size, out.size());
    EXPECT_EQ(0, memcmp(&out[0], gsm7_test_data[i].packed_gsm7, out.size()));
  }
}

TEST(Utilities, Utf8Gsm7RoundTrip) {
  using utilities::Utf8StringToGsm7;
  using utilities::Gsm7ToUtf8String;
  std::vector<uint8_t> gsm7_out;
  std::string utf8_out;

  for (int i = 0; gsm7_test_data[i].packed_gsm7 != nullptr; ++i) {
    gsm7_out = Utf8StringToGsm7(gsm7_test_data[i].utf8_string);
    utf8_out = Gsm7ToUtf8String(&gsm7_out[1],
                                gsm7_out.size() - 1,
                                gsm7_out[0],
                                0);
    EXPECT_EQ(gsm7_test_data[i].utf8_string, utf8_out);
  }
}

TEST(Utilities, Gsm7Utf8RoundTrip) {
  using utilities::Utf8StringToGsm7;
  using utilities::Gsm7ToUtf8String;
  std::vector<uint8_t> gsm7_out;
  std::string utf8_out;

  for (int i = 0; gsm7_test_data[i].packed_gsm7 != nullptr; ++i) {
    utf8_out = Gsm7ToUtf8String(&gsm7_test_data[i].packed_gsm7[1],
                                gsm7_test_data[i].packed_gsm7_size - 1,
                                gsm7_test_data[i].packed_gsm7[0],
                                0);
    gsm7_out = Utf8StringToGsm7(utf8_out);
    EXPECT_EQ(gsm7_test_data[i].packed_gsm7_size, gsm7_out.size());
    EXPECT_EQ(
        0, memcmp(&gsm7_out[0], gsm7_test_data[i].packed_gsm7,
                  gsm7_out.size()));
  }
}

// packed GSM-7 encoding starting at a 3-bit offset
// hand-constructed
const uint8_t gsm1_bit_offset_3[] = {
  10, 0x40, 0x97, 0xd9, 0xec, 0x37, 0xba, 0xcc, 0x66, 0xbf, 0x01
};

TEST(Utilities, Gsm7ToUtf8BitOffset) {
  using utilities::Gsm7ToUtf8String;
  std::string out;

  out = Gsm7ToUtf8String(&gsm1_bit_offset_3[1],
                         sizeof(gsm1_bit_offset_3) - 1,
                         gsm1_bit_offset_3[0],
                         3);
  EXPECT_EQ("hellohello", out);
}

// packed GSM-7 encoding starting at a 1-bit offset
// taken from data seen in the wild (second part of a long
// "hellohellohello..."  message)
const uint8_t gsm1_bit_offset_1[] = {
  17, 0xd8, 0x6f, 0x74, 0x99, 0xcd, 0x7e, 0xa3, 0xcb, 0x6c, 0xf6, 0x1b, 0x5d,
  0x66, 0xb3, 0xdf
};

TEST(Utilities, Gsm7ToUtf8BitOffset1) {
  using utilities::Gsm7ToUtf8String;
  std::string out;

  out = Gsm7ToUtf8String(&gsm1_bit_offset_1[1],
                         sizeof(gsm1_bit_offset_1) - 1,
                         gsm1_bit_offset_1[0],
                         1);
  EXPECT_EQ("lohellohellohello", out);
}

TEST(Utilities, Gsm7InvalidCharacter) {
  using utilities::Utf8StringToGsm7;
  using utilities::Gsm7ToUtf8String;
  std::string utf8_input;
  std::vector<uint8_t> gsm7_out;
  std::string utf8_out;

  utf8_input = "This |±| text '©' has |½| non-GSM7 characters";
  gsm7_out = Utf8StringToGsm7(utf8_input);
  utf8_out = Gsm7ToUtf8String(&gsm7_out[1],
                              gsm7_out.size() - 1,
                              gsm7_out[0],
                              0);
  // Expect the text to have spaces where the invalid characters were.
  EXPECT_EQ("This | | text ' ' has | | non-GSM7 characters", utf8_out);
}

uint8_t ucs_sample1[] = {
  0x3a,
  0x04, 0x1f, 0x04, 0x40, 0x04, 0x3e, 0x04, 0x41,
  0x04, 0x42, 0x04, 0x3e, 0x00, 0x20, 0x04, 0x42,
  0x04, 0x35, 0x04, 0x3a, 0x04, 0x41, 0x04, 0x42,
  0x00, 0x2e, 0x00, 0x20, 0x00, 0x4a, 0x00, 0x75,
  0x00, 0x73, 0x00, 0x74, 0x00, 0x20, 0x00, 0x73,
  0x00, 0x6f, 0x00, 0x6d, 0x00, 0x65, 0x00, 0x20,
  0x00, 0x74, 0x00, 0x65, 0x00, 0x78, 0x00, 0x74,
  0x00, 0x2e
};

uint8_t ucs_sample2[] = {
  0x08,
  0x04, 0x42, 0x04, 0x35, 0x04, 0x41, 0x04, 0x42
};

static const struct {
  const std::string utf8_string;
  const uint8_t* ucs2_string;
  size_t ucs2_size;
} ucs2_test_data[] = {
  {"Просто текст. Just some text.", ucs_sample1, sizeof(ucs_sample1)},
  {"тест", ucs_sample2, sizeof(ucs_sample2)},
  {"", nullptr}
};

TEST(Utilities, Ucs2ToUtf8) {
  using utilities::Ucs2ToUtf8String;
  std::string out;

  for (int i = 0; ucs2_test_data[i].ucs2_string != nullptr; ++i) {
    out = Ucs2ToUtf8String(&ucs2_test_data[i].ucs2_string[1],
                           ucs2_test_data[i].ucs2_string[0]);
    EXPECT_EQ(ucs2_test_data[i].utf8_string, out);
  }
}

TEST(Utilities, Utf8ToUcs2) {
  using utilities::Utf8StringToUcs2;
  std::vector<uint8_t> out;

  for (int i = 0; ucs2_test_data[i].ucs2_string != nullptr; ++i) {
    out = Utf8StringToUcs2(ucs2_test_data[i].utf8_string);
    EXPECT_EQ(ucs2_test_data[i].ucs2_size, out.size());
    EXPECT_EQ(0, memcmp(&out[0], ucs2_test_data[i].ucs2_string, out.size()));
  }
}

TEST(Utilities, Utf8Ucs2RoundTrip) {
  using utilities::Utf8StringToUcs2;
  using utilities::Ucs2ToUtf8String;
  std::vector<uint8_t> ucs2_out;
  std::string utf8_out;

  for (int i = 0; ucs2_test_data[i].ucs2_string != nullptr; ++i) {
    ucs2_out = Utf8StringToUcs2(ucs2_test_data[i].utf8_string);
    utf8_out = Ucs2ToUtf8String(&ucs2_out[1], ucs2_out[0]);
    EXPECT_EQ(ucs2_test_data[i].utf8_string, utf8_out);
  }
}

TEST(Utilities, Ucs2Utf8RoundTrip) {
  using utilities::Utf8StringToUcs2;
  using utilities::Ucs2ToUtf8String;
  std::vector<uint8_t> ucs2_out;
  std::string utf8_out;

  for (int i = 0; ucs2_test_data[i].ucs2_string != nullptr; ++i) {
    utf8_out = Ucs2ToUtf8String(&ucs2_test_data[i].ucs2_string[1],
                                ucs2_test_data[i].ucs2_string[0]);
    ucs2_out = Utf8StringToUcs2(utf8_out);
    EXPECT_EQ(ucs2_test_data[i].ucs2_size, ucs2_out.size());
    EXPECT_EQ(
        0, memcmp(&ucs2_out[0], ucs2_test_data[i].ucs2_string,
                  ucs2_out.size()));
  }
}
