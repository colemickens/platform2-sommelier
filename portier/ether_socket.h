// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_ETHER_SOCKET_H_
#define PORTIER_ETHER_SOCKET_H_

#include <linux/filter.h>

#include <memory>
#include <string>

#include <shill/net/byte_string.h>
#include <shill/net/ip_address.h>

#include "portier/ll_address.h"
#include "portier/network_socket.h"
#include "portier/status.h"

namespace portier {

struct IPv6EtherHeader {
  // Ethernet fields.
  LLAddress destination_ll_address;
  LLAddress source_ll_address;
  // IPv6 fields.
  // -  |ip6_header_flow| contain the values of the IP version, Traffic Class
  //    and Flow Label from the first 4 oclets of the IPv6 packet.  The
  //    information is store as is in network byte order.  This information
  //    is intended to be passed to another EtherSocket when sending a proxied
  //    packet.  Use of Traffic Class and Flow Label depend on the network
  //    and not providing them on proxied packets may result in routing issues
  //    or network firewall actions.
  uint32_t ip6_header_flow;
  // -  |next_header| the next header field of the IPv6 packet which describes
  //    the payload type.  This may specify a Hop-By-Hop option, in which case
  //    further processing on the payload may be required by the node.
  uint8_t next_header;
  uint8_t hop_limit;
  shill::IPAddress source_address;
  shill::IPAddress destination_address;

  IPv6EtherHeader() : ip6_header_flow(0), next_header(0), hop_limit(0) {}
};

// A wrapper around a raw ethernet socket for ethernet frames containing
// IPv6 packets.
class EtherSocket : public NetworkSocket {
 public:
  static std::unique_ptr<EtherSocket> Create(const std::string& if_name);
  ~EtherSocket() override;

  // Socket options.

  // Attach a BPF to the socket.  This will effect only incoming
  // packets.  Passing nullptr will detach the current filter.
  Status AttachFilter(const struct sock_fprog* sock_filter_prog);

  // Enabled or disable multicast mode for socket.  If enabled, the socket
  // will receive all L2 multicast and broadcast ethernet frames that are
  // detected on the network link.
  // Note: Possible that multicast packets are discarded even if this option
  // is enabled if the provided BPF rejects them.
  Status SetAllMulticastMode(bool enabled);
  // Enabled or disable promiscuous mode for the socket.  Should enable
  // receiving all ethernet packets on the same link.  Not all link-layer
  // networks support promiscuous mode (such as WiFi) and some L2 switches
  // may not forward unicast packets to all interfaces.
  // Note: Possible that packets are discarded even if this option is
  // enabled if the provided BPF rejects them.
  Status SetPromiscuousMode(bool enabled);

  // Receive an ether frame.
  // The received frame will be partily parsed to extract the certain fields
  // from the frame.  Both parameters are optional outputs.
  // -  |header_fields| will contain all the value extracted from the
  //    Ethernet and IPv6 header fields.
  // -  |payload| the payload of the IPv6 packet.  Depending on
  //    the value of |header_fields.next_header|, additional processing of
  //    the payload may be required.
  Status ReceiveIPv6Packet(IPv6EtherHeader* header_fields,
                           shill::ByteString* payload);

  // Discard a received, queued ether frame.
  Status DiscardPacket();

  // Send an ether frame.
  // -  |header_fields| should contain all the values required for
  //    sending an ethernet frame.  No validation is done on the fields
  //    other than ensuring the correct type of link-layer addresses and
  //    correct family of IP addresses.
  // -  |payload| the payload of the IPv6 packet.
  Status SendIPv6Packet(const IPv6EtherHeader& header_fields,
                        const shill::ByteString& payload);

 private:
  explicit EtherSocket(const std::string& if_name);
  Status Init() override;

  DISALLOW_COPY_AND_ASSIGN(EtherSocket);
};

}  // namespace portier

#endif  // PORTIER_ETHER_SOCKET_H_
