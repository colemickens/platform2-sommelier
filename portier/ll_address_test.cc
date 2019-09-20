// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/ll_address.h"

#include <linux/if_packet.h>
#include <net/if_arp.h>
#include <sys/socket.h>

#include <gtest/gtest.h>
#include <shill/net/byte_string.h>

namespace portier {

using shill::ByteString;
using Type = LLAddress::Type;

// Testing constants.
namespace {

// clang-format off

constexpr uint8_t kEui48UnicastUniversal1[] = {
  0xa0, 0x8c, 0xfd, 0xc3, 0xb3, 0xc0
};
constexpr char kEui48UnicastUniversalString1[] = "a0:8c:fd:c3:b3:c0";
const struct sockaddr_ll kEui48UnicastUniversalSockAddr1 = {
  .sll_family = AF_PACKET,
  .sll_hatype = ARPHRD_ETHER,
  .sll_halen = 6,
  .sll_addr = {
    0xa0, 0x8c, 0xfd, 0xc3, 0xb3, 0xc0
  }
};

constexpr uint8_t kEui48UnicastLocal1[] = {
  0xa2, 0x8c, 0xfd, 0xc3, 0xb3, 0xbf
};
constexpr char kEui48UnicastLocalString1[] = "a2:8c:fd:c3:b3:bf";
const struct sockaddr_ll kEui48UnicastLocalSockAddr1 = {
  .sll_family = AF_PACKET,
  .sll_hatype = ARPHRD_ETHER,
  .sll_halen = 6,
  .sll_addr = {
    0xa2, 0x8c, 0xfd, 0xc3, 0xb3, 0xbf
  }
};

constexpr uint8_t kEui48MulticastUniversal1[] = {
  0x01, 0x00, 0x0C, 0xCC, 0xCC, 0xCC
};
constexpr char kEui48MulticastUniversalString1[] = "01:00:0c:cc:cc:cc";
const struct sockaddr_ll kEui48MulticastUniversalSockAddr1 = {
  .sll_family = AF_PACKET,
  .sll_hatype = ARPHRD_ETHER,
  .sll_halen = 6,
  .sll_addr = {
    0x01, 0x00, 0x0C, 0xCC, 0xCC, 0xCC
  }
};

constexpr uint8_t kEui48MulticastLocal1[] = {
  0x33, 0x33, 0xfe, 0xdf, 0xdc, 0x4e
};
constexpr char kEui48MulticastLocalString1[] = "33:33:fe:df:dc:4e";
const struct sockaddr_ll kEui48MulticastLocalSockAddr1 = {
  .sll_family = AF_PACKET,
  .sll_hatype = ARPHRD_ETHER,
  .sll_halen = 6,
  .sll_addr = {
    0x33, 0x33, 0xfe, 0xdf, 0xdc, 0x4e
  }
};

constexpr uint8_t kEui48Broadcast[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
constexpr char kEui48BroadcastString[] = "ff:ff:ff:ff:ff:ff";
const struct sockaddr_ll kEui48BroadcastSockAddr = {
  .sll_family = AF_PACKET,
  .sll_hatype = ARPHRD_ETHER,
  .sll_halen = 6,
  .sll_addr = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff
  }
};

constexpr char kEui48MixedCaseString1[] = "eE:fF:3c:22:6A:bB";
constexpr char kEui48MixedCaseString2[] = "EE:fF:3c:22:6a:bb";
constexpr char kEui48MixedCaseString3[] = "ee:ff:3C:22:6A:BB";

// Bad address.

constexpr uint8_t kWayTooLong[] = {
  0xaa, 0xf3, 0x12, 0x32, 0x03, 0xc3, 0x86, 0xb3, 0x41, 0x96,
  0x01, 0x64, 0x0a, 0x79, 0x79, 0xa0, 0x13, 0x28, 0xf4, 0x26
};
constexpr uint8_t kWayTooShort[] = {
  0x00
};

constexpr char kEui48WithDashes[] = "11-22-33-44-55-66";
constexpr char kEui64WithDashes[] = "11-22-33-44-55-66-77-88";

constexpr char kLeadingSpaces[] = "   12:23:34:45:56:67";
constexpr char kTrailingSpaces[] = "12:23:34:45:56:67    ";
constexpr char kLeadingAndTrailingSpaces[] = " 12:23:34:45:56:67       ";
constexpr char kCenterSpaces[] = "11:22:33 :44:55:66";

constexpr char kNotAnAddress[] = "To be or not to be";
constexpr char kMixedColonsAndDashes[] = "50-ef:1f-61:d1-e7";
constexpr char kNotHexidecimal[] = "50:ef:1g:6z:d1:e7";
constexpr char kEmptyString[] = "";

constexpr char kInnerSingleCharacters[] = "ee:ee:e:ee:ee:ee";
constexpr char kLeadingSingleCharacters[] = "e:ee:ee:ee:ee:ee";
constexpr char kTrailingSingleCharacters[] = "ee:ee:ee:ee:ee:e";
constexpr char kInnerTripleCharacters[] = "ee:eee:ee:ee:ee:ee";
constexpr char kLeadingTripleCharacters[] = "eee:ee:ee:ee:ee:ee";
constexpr char kTrailingTripleCharacters[] = "ee:ee:ee:ee:ee:eee";

constexpr char kTrailingColons[] = "ee:ee:ee:ee:ee:ee:";
constexpr char kLeadingColons[] = ":ee:ee:ee:ee:ee:ee";
constexpr char kDoubleColon[] = "ee:ee:ee::ee:ee:ee";

constexpr char kWithoutLeadingZeros[] = "50:ef:f:61:1:e7";

// Valid but used for swapped types.
constexpr char kValidEui48[] = "ee:ee:ee:ee:ee:ee";
constexpr char kValidEui64[] = "ee:ee:ee:ee:ee:ee:ee:ee";

const struct sockaddr_ll kUnknownHardwareType = {
  .sll_family = AF_PACKET,
  .sll_hatype = ARPHRD_HDLC,
  .sll_halen = 6,
  .sll_addr = {
    0x27, 0xea, 0x87, 0x12, 0x86, 0xc5
  }
};
const struct sockaddr_ll kBadLengthForType = {
  .sll_family = AF_PACKET,
  .sll_hatype = ARPHRD_ETHER,
  .sll_halen = 8,
  .sll_addr = {
    0x3, 0x38, 0x73, 0x2c, 0xec, 0x1c, 0x37, 0x32
  }
};

const struct sockaddr_ll kBadLengthForStruct = {
  .sll_family = AF_PACKET,
  .sll_hatype = ARPHRD_EUI64,
  .sll_halen = 19,
  .sll_addr = {
    0x3, 0x38, 0x73, 0x2c, 0xec, 0x1c, 0x37, 0x1c
  }
};

// clang-format on

}  // namespace

TEST(LLAddressTest, EmptyInstance) {
  // Make an empty LL address and check that all methods return the expected
  // result.
  const LLAddress empty_address;

  EXPECT_FALSE(empty_address.IsValid());
  EXPECT_EQ(Type::kInvalid, empty_address.type());

  EXPECT_EQ(empty_address.GetLength(), 0);

  // Routing schemes.
  EXPECT_FALSE(empty_address.IsUnicast());
  EXPECT_FALSE(empty_address.IsMulticast());
  EXPECT_FALSE(empty_address.IsBroadcast());

  EXPECT_FALSE(empty_address.IsUniversal());
  EXPECT_FALSE(empty_address.IsLocal());

  const LLAddress other_empty_address = empty_address;

  EXPECT_FALSE(other_empty_address.Equals(empty_address));
  EXPECT_FALSE(empty_address.Equals(other_empty_address));
  EXPECT_TRUE(empty_address.Equals(empty_address));
}

// EUI-48 Tests.

TEST(LLAddressTest, Eui48FromBytes) {
  // Unicast-Universal.
  const ByteString uni_uni_address_bytes(kEui48UnicastUniversal1,
                                         sizeof(kEui48UnicastUniversal1));
  const LLAddress uni_uni_address(Type::kEui48, uni_uni_address_bytes);

  ASSERT_TRUE(uni_uni_address.IsValid());

  EXPECT_EQ(uni_uni_address.type(), Type::kEui48);
  EXPECT_EQ(uni_uni_address.GetLength(), 6);
  EXPECT_TRUE(uni_uni_address.address().Equals(uni_uni_address_bytes));

  EXPECT_TRUE(uni_uni_address.IsUnicast());
  EXPECT_FALSE(uni_uni_address.IsMulticast());
  EXPECT_FALSE(uni_uni_address.IsBroadcast());

  EXPECT_TRUE(uni_uni_address.IsUniversal());
  EXPECT_FALSE(uni_uni_address.IsLocal());

  EXPECT_EQ(uni_uni_address.ToString(), kEui48UnicastUniversalString1);
  EXPECT_TRUE(uni_uni_address.Equals(uni_uni_address));

  // Unicast-Local.
  const ByteString uni_loc_address_bytes(kEui48UnicastLocal1,
                                         sizeof(kEui48UnicastLocal1));
  const LLAddress uni_loc_address(Type::kEui48, uni_loc_address_bytes);

  ASSERT_TRUE(uni_loc_address.IsValid());

  EXPECT_EQ(uni_loc_address.type(), Type::kEui48);
  EXPECT_EQ(uni_loc_address.GetLength(), 6);
  EXPECT_TRUE(uni_loc_address.address().Equals(uni_loc_address_bytes));

  EXPECT_TRUE(uni_loc_address.IsUnicast());
  EXPECT_FALSE(uni_loc_address.IsMulticast());
  EXPECT_FALSE(uni_loc_address.IsBroadcast());

  EXPECT_FALSE(uni_loc_address.IsUniversal());
  EXPECT_TRUE(uni_loc_address.IsLocal());

  EXPECT_EQ(uni_loc_address.ToString(), kEui48UnicastLocalString1);
  EXPECT_TRUE(uni_loc_address.Equals(uni_loc_address));

  // Multi-Universal.
  const ByteString multi_uni_address_bytes(kEui48MulticastUniversal1,
                                           sizeof(kEui48MulticastUniversal1));
  const LLAddress multi_uni_address(Type::kEui48, multi_uni_address_bytes);

  ASSERT_TRUE(multi_uni_address.IsValid());

  EXPECT_EQ(multi_uni_address.type(), Type::kEui48);
  EXPECT_EQ(multi_uni_address.GetLength(), 6);
  EXPECT_TRUE(multi_uni_address.address().Equals(multi_uni_address_bytes));

  EXPECT_FALSE(multi_uni_address.IsUnicast());
  EXPECT_TRUE(multi_uni_address.IsMulticast());
  EXPECT_FALSE(multi_uni_address.IsBroadcast());

  EXPECT_TRUE(multi_uni_address.IsUniversal());
  EXPECT_FALSE(multi_uni_address.IsLocal());

  EXPECT_EQ(multi_uni_address.ToString(), kEui48MulticastUniversalString1);
  EXPECT_TRUE(multi_uni_address.Equals(multi_uni_address));

  // Multi-Local.
  const ByteString multi_loc_address_bytes(kEui48MulticastLocal1,
                                           sizeof(kEui48MulticastLocal1));
  const LLAddress multi_loc_address(Type::kEui48, multi_loc_address_bytes);

  ASSERT_TRUE(multi_loc_address.IsValid());

  EXPECT_EQ(multi_loc_address.type(), Type::kEui48);
  EXPECT_EQ(multi_loc_address.GetLength(), 6);
  EXPECT_TRUE(multi_loc_address.address().Equals(multi_loc_address_bytes));

  EXPECT_FALSE(multi_loc_address.IsUnicast());
  EXPECT_TRUE(multi_loc_address.IsMulticast());
  EXPECT_FALSE(multi_loc_address.IsBroadcast());

  EXPECT_FALSE(multi_loc_address.IsUniversal());
  EXPECT_TRUE(multi_loc_address.IsLocal());

  EXPECT_EQ(multi_loc_address.ToString(), kEui48MulticastLocalString1);
  EXPECT_TRUE(multi_loc_address.Equals(multi_loc_address));

  // Broadcast.
  const ByteString broadcast_address_bytes(kEui48Broadcast,
                                           sizeof(kEui48Broadcast));
  const LLAddress broadcast_address(Type::kEui48, broadcast_address_bytes);

  ASSERT_TRUE(broadcast_address.IsValid());

  EXPECT_EQ(broadcast_address.type(), Type::kEui48);
  EXPECT_EQ(broadcast_address.GetLength(), 6);
  EXPECT_TRUE(broadcast_address.address().Equals(broadcast_address_bytes));

  EXPECT_FALSE(broadcast_address.IsUnicast());
  EXPECT_TRUE(broadcast_address.IsMulticast());
  EXPECT_TRUE(broadcast_address.IsBroadcast());

  EXPECT_FALSE(broadcast_address.IsUniversal());
  EXPECT_TRUE(broadcast_address.IsLocal());

  EXPECT_EQ(broadcast_address.ToString(), kEui48BroadcastString);
  EXPECT_TRUE(broadcast_address.Equals(broadcast_address));

  // Comparisons.
  EXPECT_FALSE(uni_uni_address.Equals(uni_loc_address));
  EXPECT_FALSE(uni_uni_address.Equals(multi_uni_address));
  EXPECT_FALSE(uni_uni_address.Equals(multi_loc_address));
  EXPECT_FALSE(uni_uni_address.Equals(broadcast_address));

  EXPECT_FALSE(uni_loc_address.Equals(multi_uni_address));
  EXPECT_FALSE(uni_loc_address.Equals(multi_loc_address));
  EXPECT_FALSE(uni_loc_address.Equals(broadcast_address));

  EXPECT_FALSE(multi_uni_address.Equals(multi_loc_address));
  EXPECT_FALSE(multi_uni_address.Equals(broadcast_address));

  EXPECT_FALSE(multi_loc_address.Equals(broadcast_address));
}

TEST(LLAddressTest, Eui48FromOthers) {
  // Test that each constructor call results in the same object as the
  // byte derived address.  It is assumed that the byte derived address
  // is correct as it was tested in the Eui48FromBytes testcase.
  // Unicast-Universal.

  const LLAddress uni_uni_address(
      Type::kEui48,
      ByteString(kEui48UnicastUniversal1, sizeof(kEui48UnicastUniversal1)));
  const LLAddress uni_uni_str_address(Type::kEui48,
                                      kEui48UnicastUniversalString1);
  const LLAddress uni_uni_ll_struct_address(&kEui48UnicastUniversalSockAddr1);

  ASSERT_TRUE(uni_uni_address.IsValid());
  EXPECT_TRUE(uni_uni_str_address.IsValid());
  EXPECT_TRUE(uni_uni_ll_struct_address.IsValid());

  EXPECT_EQ(uni_uni_ll_struct_address.type(), Type::kEui48);

  EXPECT_TRUE(uni_uni_address.Equals(uni_uni_str_address));
  EXPECT_TRUE(uni_uni_address.Equals(uni_uni_ll_struct_address));

  // Unicast-Local.
  const LLAddress uni_loc_address(
      Type::kEui48,
      ByteString(kEui48UnicastLocal1, sizeof(kEui48UnicastLocal1)));
  const LLAddress uni_loc_str_address(Type::kEui48, kEui48UnicastLocalString1);
  const LLAddress uni_loc_ll_struct_address(&kEui48UnicastLocalSockAddr1);

  ASSERT_TRUE(uni_loc_address.IsValid());
  EXPECT_TRUE(uni_loc_str_address.IsValid());
  EXPECT_TRUE(uni_loc_ll_struct_address.IsValid());

  EXPECT_EQ(uni_loc_ll_struct_address.type(), Type::kEui48);

  EXPECT_TRUE(uni_loc_address.Equals(uni_loc_str_address));
  EXPECT_TRUE(uni_loc_address.Equals(uni_loc_ll_struct_address));

  // Multi-Universal.
  const LLAddress multi_uni_address(
      Type::kEui48,
      ByteString(kEui48MulticastUniversal1, sizeof(kEui48MulticastUniversal1)));
  const LLAddress multi_uni_str_address(Type::kEui48,
                                        kEui48MulticastUniversalString1);
  const LLAddress multi_uni_ll_struct_address(
      &kEui48MulticastUniversalSockAddr1);

  ASSERT_TRUE(multi_uni_address.IsValid());
  EXPECT_TRUE(multi_uni_str_address.IsValid());
  EXPECT_TRUE(multi_uni_ll_struct_address.IsValid());

  EXPECT_EQ(multi_uni_ll_struct_address.type(), Type::kEui48);

  EXPECT_TRUE(multi_uni_address.Equals(multi_uni_str_address));
  EXPECT_TRUE(multi_uni_address.Equals(multi_uni_ll_struct_address));

  // Multi-Local.
  const LLAddress multi_loc_address(
      Type::kEui48,
      ByteString(kEui48MulticastLocal1, sizeof(kEui48MulticastLocal1)));
  const LLAddress multi_loc_str_address(Type::kEui48,
                                        kEui48MulticastLocalString1);
  const LLAddress multi_loc_ll_struct_address(&kEui48MulticastLocalSockAddr1);

  ASSERT_TRUE(multi_loc_address.IsValid());
  EXPECT_TRUE(multi_loc_str_address.IsValid());
  EXPECT_TRUE(multi_loc_ll_struct_address.IsValid());

  EXPECT_EQ(multi_loc_ll_struct_address.type(), Type::kEui48);

  EXPECT_TRUE(multi_loc_address.Equals(multi_loc_str_address));
  EXPECT_TRUE(multi_loc_address.Equals(multi_loc_ll_struct_address));

  // Broadcast.
  const LLAddress broadcast_address(
      Type::kEui48, ByteString(kEui48Broadcast, sizeof(kEui48Broadcast)));
  const LLAddress broadcast_str_address(Type::kEui48, kEui48BroadcastString);
  const LLAddress broadcast_ll_struct_address(&kEui48BroadcastSockAddr);

  ASSERT_TRUE(broadcast_address.IsValid());
  EXPECT_TRUE(broadcast_str_address.IsValid());
  EXPECT_TRUE(broadcast_ll_struct_address.IsValid());

  EXPECT_EQ(broadcast_ll_struct_address.type(), Type::kEui48);

  EXPECT_TRUE(broadcast_address.Equals(broadcast_str_address));
  EXPECT_TRUE(broadcast_address.Equals(broadcast_ll_struct_address));
}

TEST(LLAddressTest, Eui48MixedCase) {
  // Valid but mixed lowercase and uppercase
  const LLAddress address1(Type::kEui48, kEui48MixedCaseString1);
  const LLAddress address2(Type::kEui48, kEui48MixedCaseString2);
  const LLAddress address3(Type::kEui48, kEui48MixedCaseString3);

  ASSERT_TRUE(address1.IsValid());
  ASSERT_TRUE(address2.IsValid());
  ASSERT_TRUE(address3.IsValid());

  EXPECT_TRUE(address1.Equals(address2));
  EXPECT_TRUE(address1.Equals(address3));
  EXPECT_TRUE(address2.Equals(address1));
  EXPECT_TRUE(address2.Equals(address3));
  EXPECT_TRUE(address3.Equals(address1));
  EXPECT_TRUE(address3.Equals(address2));
}

TEST(LLAddressTest, EuiWithDashes) {
  const LLAddress eui_48_with_dashes(Type::kEui48, kEui48WithDashes);
  const LLAddress eui_64_with_dashes(Type::kEui64, kEui64WithDashes);

  EXPECT_FALSE(eui_48_with_dashes.IsValid());
  EXPECT_FALSE(eui_64_with_dashes.IsValid());
}

TEST(LLAddressTest, Eui48WithSpaces) {
  const LLAddress leading_spaces_address(Type::kEui48, kLeadingSpaces);
  const LLAddress trailing_spaces_address(Type::kEui48, kTrailingSpaces);
  const LLAddress leading_and_trailing_spaces_address(
      Type::kEui48, kLeadingAndTrailingSpaces);
  const LLAddress center_spaces_address(Type::kEui48, kCenterSpaces);

  EXPECT_FALSE(leading_spaces_address.IsValid());
  EXPECT_FALSE(trailing_spaces_address.IsValid());
  EXPECT_FALSE(leading_and_trailing_spaces_address.IsValid());
  EXPECT_FALSE(center_spaces_address.IsValid());
}

TEST(LLAddressTest, Eui48BadStrings) {
  // Invalid strings.
  const LLAddress not_an_address(Type::kEui48, kNotAnAddress);
  const LLAddress mixed_col_n_dash_address(Type::kEui48, kMixedColonsAndDashes);
  const LLAddress not_hex_address(Type::kEui48, kNotHexidecimal);
  const LLAddress empty_string_address(Type::kEui48, kEmptyString);

  EXPECT_FALSE(not_an_address.IsValid());
  EXPECT_FALSE(mixed_col_n_dash_address.IsValid());
  EXPECT_FALSE(not_hex_address.IsValid());
  EXPECT_FALSE(empty_string_address.IsValid());
}

TEST(LLAddressTest, InvalidHexOclet) {
  const LLAddress inner_single_character_address(Type::kEui48,
                                                 kInnerSingleCharacters);
  const LLAddress leading_single_character_address(Type::kEui48,
                                                   kLeadingSingleCharacters);
  const LLAddress trailing_single_character_address(Type::kEui48,
                                                    kTrailingSingleCharacters);
  const LLAddress inner_triple_character_address(Type::kEui48,
                                                 kInnerTripleCharacters);
  const LLAddress leading_triple_character_address(Type::kEui48,
                                                   kLeadingTripleCharacters);
  const LLAddress trailing_triple_character_address(Type::kEui48,
                                                    kTrailingTripleCharacters);

  EXPECT_FALSE(inner_single_character_address.IsValid());
  EXPECT_FALSE(leading_single_character_address.IsValid());
  EXPECT_FALSE(trailing_single_character_address.IsValid());
  EXPECT_FALSE(inner_triple_character_address.IsValid());
  EXPECT_FALSE(leading_triple_character_address.IsValid());
  EXPECT_FALSE(trailing_triple_character_address.IsValid());
}

TEST(LLAddressTest, InvalidOcletSeparator) {
  const LLAddress trailing_colon_address(Type::kEui48, kTrailingColons);
  const LLAddress leading_colon_address(Type::kEui48, kLeadingColons);
  const LLAddress double_colon_address(Type::kEui48, kDoubleColon);

  EXPECT_FALSE(trailing_colon_address.IsValid());
  EXPECT_FALSE(leading_colon_address.IsValid());
  EXPECT_FALSE(double_colon_address.IsValid());
}

TEST(LLAddressTest, Eui48WithoutLeadingZeros) {
  const LLAddress without_leading_zeros(Type::kEui48, kWithoutLeadingZeros);

  EXPECT_FALSE(without_leading_zeros.IsValid());
}

TEST(LLAddressTest, SwappedEuiTypes) {
  const LLAddress eui_48_from_64(Type::kEui48, kValidEui64);
  const LLAddress eui_64_from_48(Type::kEui64, kValidEui48);

  EXPECT_FALSE(eui_48_from_64.IsValid());
  EXPECT_FALSE(eui_64_from_48.IsValid());
}

TEST(LLAddressTest, BadSizeBytes) {
  // Invalid bytes.
  const LLAddress eui_48_too_short(
      Type::kEui48, ByteString(kWayTooShort, sizeof(kWayTooShort)));
  const LLAddress eui_64_too_short(
      Type::kEui64, ByteString(kWayTooShort, sizeof(kWayTooShort)));

  const LLAddress eui_48_too_long(Type::kEui48,
                                  ByteString(kWayTooLong, sizeof(kWayTooLong)));
  const LLAddress eui_64_too_long(Type::kEui64,
                                  ByteString(kWayTooLong, sizeof(kWayTooLong)));

  EXPECT_FALSE(eui_48_too_short.IsValid());
  EXPECT_FALSE(eui_64_too_short.IsValid());
  EXPECT_FALSE(eui_48_too_long.IsValid());
  EXPECT_FALSE(eui_64_too_long.IsValid());
}

TEST(LLAddressTest, BadSockAddrStructs) {
  // Bad sockaddr_ll structs.
  const LLAddress unknown_hardware_address(&kUnknownHardwareType);
  const LLAddress bad_length_for_type_address(&kBadLengthForType);
  const LLAddress bad_length_for_struct(&kBadLengthForStruct);

  EXPECT_FALSE(unknown_hardware_address.IsValid());
  EXPECT_FALSE(bad_length_for_type_address.IsValid());
  EXPECT_FALSE(bad_length_for_struct.IsValid());
}

}  // namespace portier
