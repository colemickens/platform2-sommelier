// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/icmpv6_socket.h"

#include <errno.h>
#include <net/ethernet.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <string.h>
#include <sys/socket.h>

#include <algorithm>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/posix/safe_strerror.h>

namespace portier {

using std::string;
using std::unique_ptr;

using base::safe_strerror;
using shill::ByteString;
using shill::IPAddress;

using Code = Status::Code;
using State = ICMPv6Socket::State;

namespace {

// Assumes that the MTU for Ethernet frames are not larger than 1500
// bytes.  Not true for Jumbograms, but this case is not supported.
constexpr size_t kReceiveBufferSize = 2048;

// The minimum MTU (in bytes) allowed for a link used on an IPv6 network.
// This value is defined in RFC8200.
constexpr size_t kIPv6MinimumMTU = 1280;

// The maximum number of bytes that the message component of an ICMPv6
// can be to ensure that the entire ethernet frame is less than the
// minimum MTU of link used on an IPv6 network.
constexpr size_t kICMPv6PayloadMax =
    kIPv6MinimumMTU - (sizeof(struct ether_header) + sizeof(struct ip6_hdr) +
                       sizeof(struct icmp6_hdr));

void ConstructIPv6Header(const IPv6EtherHeader& header,
                         uint16_t payload_length,
                         struct ip6_hdr* ip6_hdr_out) {
  DCHECK_EQ(header.source_address.family(), IPAddress::kFamilyIPv6);
  DCHECK_EQ(header.destination_address.family(), IPAddress::kFamilyIPv6);
  DCHECK(ip6_hdr_out);
  ip6_hdr_out->ip6_flow = header.ip6_header_flow;
  ip6_hdr_out->ip6_plen = htons(payload_length);
  ip6_hdr_out->ip6_nxt = header.next_header;
  ip6_hdr_out->ip6_hops = header.hop_limit;
  memcpy(ip6_hdr_out->ip6_src.s6_addr, header.source_address.GetConstData(),
         sizeof(ip6_hdr_out->ip6_src.s6_addr));
  memcpy(ip6_hdr_out->ip6_dst.s6_addr,
         header.destination_address.GetConstData(),
         sizeof(ip6_hdr_out->ip6_dst.s6_addr));
}

}  // namespace

ICMPv6Socket::ICMPv6Socket(const string& if_name) : NetworkSocket(if_name) {}

unique_ptr<ICMPv6Socket> ICMPv6Socket::Create(const string& if_name) {
  unique_ptr<ICMPv6Socket> icmpv6_socket(new ICMPv6Socket(if_name));

  Status status = icmpv6_socket->Init();
  if (!status) {
    status << "Failed to initialize ICMPv6 socket for interface " << if_name;
    LOG(ERROR) << status;
    icmpv6_socket.reset();
  }

  return icmpv6_socket;
}

Status ICMPv6Socket::Init() {
  CHECK_EQ(state(), State::UNINITIALIZED);

  if (name().empty()) {
    return Status(Code::INVALID_ARGUMENT,
                  "Empty string is not a valid interface name");
  }

  // Get interface index.
  const int if_index = if_nametoindex(name().c_str());
  if (if_index < 0) {
    if (ENODEV == errno) {
      return Status(Code::DOES_NOT_EXIST)
             << "No interface found with given name: " << name();
    }
    return Status(Code::UNEXPECTED_FAILURE)
           << "if_nametoindex(): " << safe_strerror(errno);
  }
  index_ = if_index;

  const int icmp_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  if (icmp_fd < 0) {
    if (EACCES == errno) {
      return Status(Code::BAD_PERMISSIONS) << "Process does not have "
                                              "permission to open a raw "
                                              "ICMPv6 socket";
    }
    return Status(Code::UNEXPECTED_FAILURE)
           << "socket(): " << safe_strerror(errno);
  }
  set_fd(icmp_fd);

  // Bind socket to interface.
  struct ifreq icmp_ifr;
  PrepareIfRequestStruct(&icmp_ifr);
  if (setsockopt(icmp_fd, SOL_SOCKET, SO_BINDTODEVICE, &icmp_ifr,
                 sizeof(icmp_ifr)) < 0) {
    CloseFd();
    if (EACCES == errno) {
      return Status(Code::BAD_PERMISSIONS)
             << "Process does not have permission to bind to interface";
    }
    if (EADDRINUSE == errno) {
      return Status(Code::RESOURCE_IN_USE)
             << "Interface " << name() << " is already bound to another socket";
    }
    return Status(Code::UNEXPECTED_FAILURE)
           << "Binding ICMPv6 using setsockopt(): " << safe_strerror(errno);
  }
  state_ = State::READY;

  return Status();
}

ICMPv6Socket::~ICMPv6Socket() {
  if (IsReady()) {
    Close();
  } else if (IsUnitialized() && fd() != -1) {
    CloseFd();
    state_ = State::CLOSED;
  }
}

Status ICMPv6Socket::AttachFilter(const struct icmp6_filter* icmp6_filter) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  // If |icmp6_filter| is null, then the socket filter is set as PASSALL.
  struct icmp6_filter icmp6_pass_all_filter;
  if (icmp6_filter == nullptr) {
    ICMP6_FILTER_SETPASSALL(&icmp6_pass_all_filter);
    icmp6_filter = &icmp6_pass_all_filter;
  }

  if (setsockopt(fd(), IPPROTO_ICMPV6, ICMP6_FILTER, icmp6_filter,
                 sizeof(struct icmp6_filter)) < 0) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Attaching ICMPv6 socket filter setsockopt(): "
           << safe_strerror(errno);
  }

  return Status();
}

Status ICMPv6Socket::SetMulticastHopLimit(uint8_t hop_limit) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  // Socket option must be an integer.
  const int hop_limit_int = static_cast<int>(hop_limit);
  if (setsockopt(fd(), IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hop_limit_int,
                 sizeof(hop_limit_int)) < 0) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Setting ICMPv6 multicast hop limit setsockopt(): "
           << safe_strerror(errno);
  }

  return Status();
}

Status ICMPv6Socket::SetUnicastHopLimit(uint8_t hop_limit) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  // Socket option must be an integer.
  const int hop_limit_int = static_cast<int>(hop_limit);

  if (setsockopt(fd(), IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hop_limit_int,
                 sizeof(hop_limit_int)) < 0) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Setting ICMPv6 multicast hop limit setsockopt(): "
           << safe_strerror(errno);
  }

  return Status();
}

// Sending and receiving.

Status ICMPv6Socket::ReceiveMessage(ICMPv6Header* header_fields,
                                    shill::ByteString* message_body) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  uint8_t buffer[kReceiveBufferSize];
  struct sockaddr_in6 source_info = {};
  socklen_t source_info_len = sizeof(struct sockaddr_in6);
  const int32_t res =
      HANDLE_EINTR(recvfrom(fd(), buffer, kReceiveBufferSize, 0,
                            (struct sockaddr*)&source_info, &source_info_len));

  if (res < 0) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to receive ICMPv6 packet: recvfrom(): "
           << safe_strerror(errno);
  }
  if (res == 0) {
    return Status(Code::RESULT_UNAVAILABLE, "Nothing was received");
  }
  if (res < sizeof(struct icmp6_hdr)) {
    return Status(Code::MALFORMED_PACKET, "Packet was truncated");
  }

  if (source_info.sin6_family != AF_INET6) {
    return Status(Code::UNEXPECTED_FAILURE, "Received non-IPv6 packet");
  }

  const uint8_t* const icmp6_ptr = buffer;
  const struct icmp6_hdr* icmp6_hdr =
      reinterpret_cast<const struct icmp6_hdr*>(icmp6_ptr);

  if (header_fields != nullptr) {
    header_fields->remote_address = IPAddress(
        IPAddress::kFamilyIPv6, ByteString(source_info.sin6_addr.s6_addr,
                                           sizeof(source_info.sin6_addr)));
    header_fields->type = icmp6_hdr->icmp6_type;
    header_fields->code = icmp6_hdr->icmp6_code;
    memcpy(header_fields->data, icmp6_hdr->icmp6_data8,
           sizeof(header_fields->data));
  }

  const uint8_t* const message_ptr = icmp6_ptr + sizeof(struct icmp6_hdr);
  const int32_t message_len = res - sizeof(struct icmp6_hdr);
  if (message_body) {
    if (message_len == 0) {
      message_body->Clear();
    } else {
      *message_body = ByteString(message_ptr, message_len);
    }
  }

  return Status();
}

Status ICMPv6Socket::DiscardMessage() {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  // Discarding the packet requires that the system call succeeds.
  uint8_t buffer[kReceiveBufferSize];
  struct sockaddr_in6 source_info = {};
  socklen_t source_info_len = sizeof(struct sockaddr_in6);
  const int32_t res =
      HANDLE_EINTR(recvfrom(fd(), buffer, kReceiveBufferSize, 0,
                            (struct sockaddr*)&source_info, &source_info_len));
  if (res < 0) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to discard ICMPv6 packet: recvfrom(): "
           << safe_strerror(errno);
  }
  return Status();
}

Status ICMPv6Socket::SendMessage(const ICMPv6Header& header_fields,
                                 const ByteString& message_body) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  struct msghdr message_header = {};
  struct iovec message_parts[2];  // ICMP Header + message
  message_header.msg_iov = message_parts;
  message_header.msg_iovlen = 1;

  // Prepare destination address.
  struct sockaddr_in6 dest_info = {};
  dest_info.sin6_family = AF_INET6;
  memcpy(dest_info.sin6_addr.s6_addr,
         header_fields.remote_address.GetConstData(),
         sizeof(dest_info.sin6_addr));
  message_header.msg_name = &dest_info;
  message_header.msg_namelen = sizeof(struct sockaddr_in6);

  // Prepare packet, starting with the ICMPv6 header.
  struct icmp6_hdr icmp6_hdr = {};
  icmp6_hdr.icmp6_type = header_fields.type;
  icmp6_hdr.icmp6_code = header_fields.code;
  // By setting to zero, kernel will fill in value.
  icmp6_hdr.icmp6_cksum = 0;
  memcpy(icmp6_hdr.icmp6_data8, header_fields.data,
         sizeof(icmp6_hdr.icmp6_data8));
  message_parts[0].iov_base = &icmp6_hdr;
  message_parts[0].iov_len = sizeof(struct icmp6_hdr);

  // Add message body.
  if (message_body.GetLength() > 0) {
    message_parts[1].iov_base =
        const_cast<uint8_t*>(message_body.GetConstData());
    message_parts[1].iov_len = message_body.GetLength();
    message_header.msg_iovlen++;
  }

  const int32_t res = HANDLE_EINTR(sendmsg(fd(), &message_header, 0));
  if (res < 0) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to send ICMPv6 packet: sendto(): "
           << safe_strerror(errno);
  }

  return Status();
}

Status ICMPv6Socket::SendDestinationUnreachableMessage(
    const IPAddress& destination_address,
    uint8_t code,
    const IPv6EtherHeader& original_header,
    const ByteString& original_body) {
  struct msghdr message_header = {};
  struct iovec message_parts[3];  // ICMP header + IP header + message
  message_header.msg_iov = message_parts;
  message_header.msg_iovlen = 2;

  // Prepare destination address.
  struct sockaddr_in6 dest_info = {};
  dest_info.sin6_family = AF_INET6;
  memcpy(dest_info.sin6_addr.s6_addr, destination_address.GetConstData(),
         sizeof(dest_info.sin6_addr));
  message_header.msg_name = &dest_info;
  message_header.msg_namelen = sizeof(struct sockaddr_in6);

  // Prepare packet, starting with the ICMPv6 header.
  struct icmp6_hdr icmp6_hdr = {};
  icmp6_hdr.icmp6_type = ICMP6_DST_UNREACH;
  icmp6_hdr.icmp6_code = code;
  message_parts[0].iov_base = &icmp6_hdr;
  message_parts[0].iov_len = sizeof(struct icmp6_hdr);

  // IPv6 header.
  struct ip6_hdr ip6_hdr;
  ConstructIPv6Header(original_header, original_body.GetLength(), &ip6_hdr);
  message_parts[1].iov_base = &ip6_hdr;
  message_parts[1].iov_len = sizeof(struct ip6_hdr);

  if (original_body.GetLength() > 0) {
    message_parts[2].iov_base =
        const_cast<uint8_t*>(original_body.GetConstData());
    message_parts[2].iov_len = std::min(
        original_body.GetLength(), kICMPv6PayloadMax - sizeof(struct ip6_hdr));
    message_header.msg_iovlen++;
  }

  const int32_t res = HANDLE_EINTR(sendmsg(fd(), &message_header, 0));
  if (res < 0) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to send ICMPv6 packet: sendto(): "
           << safe_strerror(errno);
  }

  return Status();
}

Status ICMPv6Socket::SendPacketTooBigMessage(
    const shill::IPAddress& destination_address,
    uint32_t mtu,
    const IPv6EtherHeader& original_header,
    const shill::ByteString& original_body) {
  struct msghdr message_header = {};
  struct iovec message_parts[3];  // ICMP header + IP header + message
  message_header.msg_iov = message_parts;
  message_header.msg_iovlen = 2;

  // Prepare destination address.
  struct sockaddr_in6 dest_info = {};
  dest_info.sin6_family = AF_INET6;
  memcpy(dest_info.sin6_addr.s6_addr, destination_address.GetConstData(),
         sizeof(dest_info.sin6_addr));
  message_header.msg_name = &dest_info;
  message_header.msg_namelen = sizeof(struct sockaddr_in6);

  // Prepare packet, starting with the ICMPv6 header.
  struct icmp6_hdr icmp6_hdr = {};
  icmp6_hdr.icmp6_type = ICMP6_PACKET_TOO_BIG;
  icmp6_hdr.icmp6_code = 0;
  icmp6_hdr.icmp6_mtu = htonl(mtu);
  message_parts[0].iov_base = &icmp6_hdr;
  message_parts[0].iov_len = sizeof(struct icmp6_hdr);

  // IPv6 header.
  struct ip6_hdr ip6_hdr;
  ConstructIPv6Header(original_header, original_body.GetLength(), &ip6_hdr);
  message_parts[1].iov_base = &ip6_hdr;
  message_parts[1].iov_len = sizeof(struct ip6_hdr);

  if (original_body.GetLength() > 0) {
    message_parts[2].iov_base =
        const_cast<uint8_t*>(original_body.GetConstData());
    message_parts[2].iov_len = std::min(
        original_body.GetLength(), kICMPv6PayloadMax - sizeof(struct ip6_hdr));
    message_header.msg_iovlen++;
  }

  const int32_t res = HANDLE_EINTR(sendmsg(fd(), &message_header, 0));
  if (res < 0) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to send ICMPv6 packet: sendto(): "
           << safe_strerror(errno);
  }

  return Status();
}

}  // namespace portier
