// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_ICMPV6_SOCKET_H_
#define PORTIER_ICMPV6_SOCKET_H_

#include <netinet/icmp6.h>

#include <memory>
#include <string>

#include <shill/net/byte_string.h>
#include <shill/net/ip_address.h>

#include "portier/ether_socket.h"
#include "portier/ll_address.h"
#include "portier/network_socket.h"
#include "portier/status.h"

namespace portier {

struct ICMPv6Header {
  // IPv6 header fields.
  // - |remote_address| the IPv6 address which
  //   is contained in the IPv6 header's Source / Destination address
  //   fields.  When receiving a message, the remote address is the
  //   address of the sending node.  When sending, the remote address
  //   should be the address of the receiving node.
  shill::IPAddress remote_address;
  // ICMPv6 header fields.
  uint8_t type;
  uint8_t code;
  // Note that checksum is not provided and is calculated by the kernel.
  // Data found in the fifth through eigth byte of the ICMPv6 packet.
  // This is typically reserved.
  uint8_t data[4];

  ICMPv6Header() : type(0), code(0), data() {}
  ~ICMPv6Header() = default;
};

// A wrapper around an ICMPv6 socket.
class ICMPv6Socket : public NetworkSocket {
 public:
  static std::unique_ptr<ICMPv6Socket> Create(const std::string& if_name);
  ~ICMPv6Socket() override;

  // Attaches ICMPv6 filter to the socket.  This filter is described in
  // detail in RFC 3542 section 3.2.  Passing NULL to this function
  // will set socket filter to PASSALL mode.
  Status AttachFilter(const struct icmp6_filter* icmp6_filter);

  // Sets the default IPv6 Hop Limit field in all outgoing ICMPv6
  // multicast packets.
  Status SetMulticastHopLimit(uint8_t hop_limit);

  // Sets the default IPv6 Hop Limit field in all outgoing ICMPv6
  // unicast packets.
  Status SetUnicastHopLimit(uint8_t hop_limit);

  // Generic sending and receiving methods.

  // Receives a raw ICMPv6 message.  Contents of |header_fields| will
  // indicate the ICMP Type, Code and first 4 byte of the message (which
  // typically contain some special information).  |message_body| will
  // contain the rest of the message after the ICMPv6 header.
  Status ReceiveMessage(ICMPv6Header* header_fields,
                        shill::ByteString* message_body);

  // Discards a message that is ready to be received.  Useful if receiving
  // unexpected ICMP messages.
  Status DiscardMessage();

  // Sends an ICMP message.  Only requirement of |header_fields| is that
  // the remove_address is IPv6.  |header_fields.type| and
  // |header_fields.code| are not checked to see if they contain valid values.
  Status SendMessage(const ICMPv6Header& header_fields,
                     const shill::ByteString& message_body);

  // Specialized sending methods.

  // Sends a ICMPv6 Destination Unreachable message.  The provided
  // |code| should be one of the values specified in RFC4443 section
  // 3.1.
  // The method is expected to be used for packets that were received
  // from an EtherSocket.  |original_header| and |original_body| will
  // be serialized to form the payload of the packet.  Regardless of
  // the size of |original_body|, the out going packet will be at most
  // the minimum IPv6 MTU (1280 bytes).
  Status SendDestinationUnreachableMessage(
      const shill::IPAddress& destination_address,
      uint8_t code,
      const IPv6EtherHeader& original_header,
      const shill::ByteString& original_body);

  // Sends a ICMPv6 Packet Too Big message.
  // The provided |mtu| should be the valud of the link MTU which
  // caused the issue.
  // The method is expected to be used for packets that were received
  // from an EtherSocket.  |original_header| and |original_body| will
  // be serialized to form the payload of the packet.  Regardless of
  // the size of |original_body|, the out going packet will be at most
  // the minimum IPv6 MTU (1280 bytes).
  Status SendPacketTooBigMessage(const shill::IPAddress& destination_address,
                                 uint32_t mtu,
                                 const IPv6EtherHeader& original_header,
                                 const shill::ByteString& original_body);

 private:
  explicit ICMPv6Socket(const std::string& if_name);
  Status Init() override;

  DISALLOW_COPY_AND_ASSIGN(ICMPv6Socket);
};

}  // namespace portier

#endif  // PORTIER_ICMPV6_SOCKET_H_
