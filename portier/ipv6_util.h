// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_IPV6_UTIL_H_
#define PORTIER_IPV6_UTIL_H_

#include <shill/net/byte_string.h>
#include <shill/net/ip_address.h>

#include "portier/ll_address.h"
#include "portier/status.h"

namespace portier {

// Calculates the 16-bit Internet checksum for IPv6 upper-layer
// protocols, such as ICMPv6.  The checksum is calculated using the
// "pseudo-header" for IPv6 as specified in RFC 8200 section 8.1.  The
// checksum is the ones-complement 16-bit sum of the data, the result
// is already in network-byte order.
//
// This function does not validate that the upper-layer data provided
// is well formed.
//
// Note: This function is optimized for actual IPv6 packets and cannot
// reliably generate a checksum for data of more than 2^16 16-bit words
// long (2^17 bytes).
//
// Important:  If you are generating the checksum for an outgoing
// packet, then the checksum field in the data bytes must be
// initialized to zero.
//
//  - |source_address| and |destination_address| must be of IPv6
//    family.
//  - |next_header| should be the Next Header value of the upper-level
//    protocol, not necessarily the same value of Next Header in the
//    actual IPv6 header.  This can occur if a Hop-by-Hop header
//    option is sent with the packet. If you are sending an ICMPv6
//    packet with Hop-by-Hop header options, |next_header| should
//    still be the value of IPPROTO_ICMPV6 and **not** the Hop-by-Hop
//    protocol number.
//  - |upper_layer_data| the data to be appended to the pseudo header
//    when calculating the checksum.
//
// Returns OK if the checksum was calculated and stored in the value
// pointed to by |checksum_out| in network byte order.  May return a
// non-OK status if the parameters are invalid.
Status IPv6UpperLayerChecksum16(const shill::IPAddress& source_address,
                                const shill::IPAddress& destination_address,
                                uint8_t next_header,
                                const uint8_t* upper_layer_data,
                                size_t data_length,
                                uint16_t* checksum_out);

Status IPv6UpperLayerChecksum16(const shill::IPAddress& source_address,
                                const shill::IPAddress& destination_address,
                                uint8_t next_header,
                                const shill::ByteString& upper_layer_data,
                                uint16_t* checksum_out);

// Check if the provied IP address is an IPv6 unspecified address (all zeros).
bool IPv6AddressIsUnspecified(const shill::IPAddress& ip_address);

// Checks if the provided address is a Link-Local address as defined in
// RFC4291: IPv6 Addressing Architecture.  Link-local IPv6 addresses
// are all addresses part of the fe80::/10 subnet.
bool IPv6AddressIsLinkLocal(const shill::IPAddress& ip_address);

// A solicited-node multicast address is of the form,
// ff02:0:0:0:0:1:ffXX:XXXX, where the trailing 24-bits are the same
// as the last 24-bits of the solicited address.
// Args:
//  - |multicast_address| The address that is tested to be a
//    solicited-node multicast address.
//  - |solicited_address| The address that is being solicited.
bool IPv6AddressIsSolicitedNodeMulticast(
    const shill::IPAddress& multicast_address,
    const shill::IPAddress& solicited_address);

// Detemines if the given IPv6 address is a multicast address as
// defined in RFC 4291: IPv6 Address Architecture, section 2.7.
// Multicast addresses are identified by belonging to the the ff00::/8
// subnet. Any address that does not start with all 1's in the first
// octet is not a multicast address.
bool IPv6AddressIsMulticast(const shill::IPAddress& multicast_address);

// Generates the multicast ethernet MAC address for IPv6 multicast
// packets.  The mulicast MAC address is defined in RFC 7042 section
// 2.3.1.  These multicast addresses lie in the range from
// 33:33:00:00:00:00 to 33:33:ff:ff:ff:ff.  The lower 32-bits of the
// MAC address are taken from the lower 32-bits of the IPv6 address.
bool IPv6GetMulticastLinkLayerAddress(const shill::IPAddress& ip_address,
                                      LLAddress* multicast_ll_out);

}  // namespace portier

#endif  // PORTIER_IPV6_UTIL_H_
