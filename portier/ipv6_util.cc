// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/ipv6_util.h"

#include <arpa/inet.h>
#include <limits.h>

#include <algorithm>

#include <base/logging.h>

namespace portier {

using shill::ByteString;
using shill::IPAddress;

namespace {

// Used to mask the lower 16 bits of a 32-bit number.
constexpr uint32_t kMask16 = 0xffff;

// Length of a EUI-48 (MAC) address.
constexpr int32_t kEui48Length = 6;
// The byte offset of the last 32-bits (4 bytes) of an IPv6 address.
constexpr int32_t kIPv6Low32BitsOffset = 12;
// Number of bytes copied from the IPv6 multicast address to the
// link-layer mulicast address.
constexpr int32_t kMulticastIPv6ComponentSize = 4;

// The address prefix for the IPv6 link-local subnet.
const IPAddress kLinkLocalSubnet("fe80::");
// IPv6 link-local subnet mask (10 bits).
const IPAddress kLinkLocalSubnetMask("ffc0::");

// Value of the first byte of multicast IPv6 addresses.
constexpr uint8_t kMulticastIdentifier = 0xff;

// The subnet and mask for solicited-node multicast addresses.
const IPAddress kSolicitedNodeSubnet("ff02:0:0:0:0:1:ff00:0");
const IPAddress kSolicitedNodeSubnetMask(
    "ffff:ffff:ffff:ffff:ffff:ffff:ff00:0");

// IPv6 Pseudo-Header.
// This struct is created in such a way that it is byte-aligned with the
// actual pseudo header format.
struct IPv6PseudoHeader {
  struct in6_addr ip6_pseudo_src;
  struct in6_addr ip6_pseudo_dst;
  uint32_t ip6_pseudo_len;  // Upper-layer data length.
  uint8_t ip6_pseudo_st_zeros[3];
  uint8_t ip6_pseudo_nxt;
};
static_assert(sizeof(IPv6PseudoHeader) == 40,
              "IPv6PseudoHeader size is not correct");

// Calculates the 16-bit one's complement of the provided data.
uint16_t InternetChecksum16(const uint8_t* data, size_t data_length) {
  uint32_t sum = 0;
  // Iterate over each 16-bits of data and add to sum.  Checking
  // (i + 1) < data_length to avoid the case where there is an odd
  // mumber of bytes.
  for (size_t i = 0; (i + 1) < data_length; i += 2) {
    const uint16_t* dptr = reinterpret_cast<const uint16_t*>(&data[i]);
    sum += static_cast<uint32_t>(*dptr);
  }

  if ((data_length & 1) == 1) {
    // Accommodate the final byte.
    uint16_t value = 0;
    // This works for both big endian vs little endian architectures.
    uint8_t* vptr = reinterpret_cast<uint8_t*>(&value);
    *vptr = data[data_length - 1];
    sum += static_cast<uint32_t>(value);
  }

  // Add all the 16-bit overflows back into the 16-bit sum.
  sum = (sum & kMask16) + (sum >> 16);
  if (sum == kMask16) {
    sum = 0;
  }

  return static_cast<uint16_t>(sum);
}

// Calculates the 16-bit one's complement sum of two numbers.
//  Note: Do not try to call this repeatedly, it is not very
//  efficient for large arrays.
uint16_t InternetChecksum16Pair(uint16_t a, uint16_t b) {
  uint32_t sum = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);
  if (sum > kMask16) {
    // Any 16-bit overflow can only have a carry of 1.
    sum = (sum & kMask16) + 1;
  }
  if (sum == kMask16) {
    sum = 0;
  }
  return static_cast<uint16_t>(sum);
}

}  // namespace

Status IPv6UpperLayerChecksum16(const IPAddress& source_address,
                                const IPAddress& destination_address,
                                uint8_t next_header,
                                const uint8_t* upper_layer_data,
                                size_t data_length,
                                uint16_t* checksum_out) {
  // A null pointer for upper_layer_data is ok if the length is zero.
  // In that case, the checksum will be of the pseudo header only.
  DCHECK(upper_layer_data == 0 || upper_layer_data != nullptr)
      << "Expected non-null value for `upper_layer_data'";
  DCHECK(checksum_out != nullptr)
      << "Expected non-null value for `checksum_out'";
  DCHECK(source_address.family() == IPAddress::kFamilyIPv6 &&
         destination_address.family() == IPAddress::kFamilyIPv6)
      << "The source and destination addresses must be IPv6";
  DCHECK(data_length < (1 << 17))
      << "Cannot accurately compute checksum for 2^17 or more bytes of data";

  // Populate the IPv6 pseudo header.
  IPv6PseudoHeader ip6_pseudo_hdr;
  memset(&ip6_pseudo_hdr, 0, sizeof(IPv6PseudoHeader));

  memcpy(&ip6_pseudo_hdr.ip6_pseudo_src, source_address.GetConstData(),
         sizeof(ip6_pseudo_hdr.ip6_pseudo_src));
  memcpy(&ip6_pseudo_hdr.ip6_pseudo_dst, destination_address.GetConstData(),
         sizeof(ip6_pseudo_hdr.ip6_pseudo_dst));
  ip6_pseudo_hdr.ip6_pseudo_len = htonl(data_length);
  ip6_pseudo_hdr.ip6_pseudo_nxt = next_header;

  // Get the checksum of the header.
  const uint16_t pseudo_checksum =
      InternetChecksum16(reinterpret_cast<const uint8_t*>(&ip6_pseudo_hdr),
                         sizeof(IPv6PseudoHeader));

  // Get the checksum of the upper layer.
  if (data_length > 0) {
    const uint16_t upper_layer_checksum =
        InternetChecksum16(upper_layer_data, data_length);
    *checksum_out =
        InternetChecksum16Pair(pseudo_checksum, upper_layer_checksum);
  } else {
    *checksum_out = pseudo_checksum;
  }

  return Status();
}

Status IPv6UpperLayerChecksum16(const IPAddress& source_address,
                                const IPAddress& destination_address,
                                uint8_t next_header,
                                const ByteString& upper_layer_data,
                                uint16_t* checksum_out) {
  return IPv6UpperLayerChecksum16(source_address, destination_address,
                                  next_header, upper_layer_data.GetConstData(),
                                  upper_layer_data.GetLength(), checksum_out);
}

bool IPv6AddressIsUnspecified(const IPAddress& ip_address) {
  DCHECK(ip_address.family() == IPAddress::kFamilyIPv6);
  const ByteString& bytes = ip_address.address();
  return std::all_of(bytes.GetConstData(),
                     bytes.GetConstData() + bytes.GetLength(),
                     [](unsigned char byte) { return (0 == byte); });
}

bool IPv6AddressIsLinkLocal(const IPAddress& ip_address) {
  DCHECK(ip_address.family() == IPAddress::kFamilyIPv6);
  return kLinkLocalSubnet.Equals(ip_address.MaskWith(kLinkLocalSubnetMask));
}

bool IPv6AddressIsSolicitedNodeMulticast(const IPAddress& multicast_address,
                                         const IPAddress& solicited_address) {
  DCHECK(multicast_address.family() == IPAddress::kFamilyIPv6);
  DCHECK(solicited_address.family() == IPAddress::kFamilyIPv6);
  // Check that |multicast_address| is of the same subnet.
  const bool solicited_node_net_match = kSolicitedNodeSubnet.Equals(
      multicast_address.MaskWith(kSolicitedNodeSubnetMask));
  // Check that the least-significant 24-bits of both address match.
  const bool solicited_bottom_bits_match =
      kSolicitedNodeSubnetMask.MergeWith(multicast_address)
          .Equals(kSolicitedNodeSubnetMask.MergeWith(solicited_address));
  // If both conditions are true, then |multicast_address| is the
  // solicited-node multicast address of |solicited_address|.
  return solicited_node_net_match && solicited_bottom_bits_match;
}

bool IPv6AddressIsMulticast(const shill::IPAddress& multicast_address) {
  DCHECK(multicast_address.family() == IPAddress::kFamilyIPv6);
  // Only the first byte needs to be checked to determine if the
  // address is a mulicast address.
  const uint8_t* first_byte = multicast_address.GetConstData();
  return *first_byte == kMulticastIdentifier;
}

bool IPv6GetMulticastLinkLayerAddress(const IPAddress& ip_address,
                                      LLAddress* multicast_ll_out) {
  DCHECK(ip_address.family() == IPAddress::kFamilyIPv6);
  DCHECK(multicast_ll_out != nullptr);
  // Form an address of 33:33:xx:xx:xx:xx.
  uint8_t raw_address[kEui48Length];
  raw_address[0] = raw_address[1] = 0x33;
  memcpy(&raw_address[2], ip_address.GetConstData() + kIPv6Low32BitsOffset,
         kMulticastIPv6ComponentSize);
  *multicast_ll_out = LLAddress(LLAddress::Type::kEui48,
                                ByteString(raw_address, sizeof(raw_address)));
  return true;
}

}  // namespace portier
