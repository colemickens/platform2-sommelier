// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/ether_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/ethernet.h>  // L2 stuff.
#include <net/if.h>        // Interface stuff.
#include <netinet/ip6.h>   // IPv6 stuff.
#include <string.h>
#include <sys/socket.h>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/posix/safe_strerror.h>
#include <base/strings/stringprintf.h>

namespace portier {

using std::string;
using std::unique_ptr;

using base::safe_strerror;

using shill::ByteString;
using shill::IPAddress;

using Code = Status::Code;
using State = EtherSocket::State;

// Constants.
namespace {

// Assumes that the MTU for Ethernet frames are not larger than 1500
// bytes.  Not true for Jumbograms, but this case is not supported.
constexpr size_t kReceiveBufferSize = 2048;

// A mask for the upper 4 bits of ip6_vfc byte containing the IP version
// from a received IP packet.
constexpr size_t kIPVersionMask = 0xf0;

// Expected IP version for IPv6.  Used specifically for comparing the IP
// version within the ip6_vfc field of an IPv6 header.
constexpr size_t kIPv6VersionBits = 0x60;

void SetBits(int16_t* flag, int16_t bits) {
  CHECK(flag != nullptr);
  *flag |= bits;
}

void ClearBits(int16_t* flag, int16_t bits) {
  CHECK(flag != nullptr);
  *flag &= ~bits;
}

}  // namespace

EtherSocket::EtherSocket(const string& if_name) : NetworkSocket(if_name) {}

unique_ptr<EtherSocket> EtherSocket::Create(const string& if_name) {
  unique_ptr<EtherSocket> ether_socket(new EtherSocket(if_name));

  Status status = ether_socket->Init();
  if (!status) {
    status << "Failed to initialize ether socket for interface " << if_name;
    LOG(ERROR) << status;
    ether_socket.reset();
  }

  return ether_socket;
}

Status EtherSocket::Init() {
  CHECK(state() == State::UNINITIALIZED);

  if (name().size() == 0) {
    return Status(Code::INVALID_ARGUMENT,
                  "Empty string is not a valid interface name");
  }

  // Get interface index.
  const int if_index = if_nametoindex(name().c_str());
  if (if_index < 0) {
    const int saved_errno = errno;
    if (ENODEV == saved_errno) {
      return Status(Code::DOES_NOT_EXIST)
             << "No interface found with given name: " << name();
    }
    return Status(Code::UNEXPECTED_FAILURE)
           << "if_nametoindex(): " << safe_strerror(saved_errno);
  }
  index_ = if_index;

  // Open raw ether socket.
  const int ether_fd = socket(AF_PACKET, SOCK_RAW, htons(ETHERTYPE_IPV6));
  if (ether_fd < 0) {
    const int saved_errno = errno;
    if (EACCES == saved_errno) {
      return Status(Code::BAD_PERMISSIONS) << "Process does not have "
                                              "permission to open a raw "
                                              "ethernet socket";
    }
    return Status(Code::UNEXPECTED_FAILURE)
           << "socket(): " << safe_strerror(saved_errno);
  }
  set_fd(ether_fd);

  // Bind socket to interface.
  struct sockaddr_ll ether_addr;
  memset(&ether_addr, 0, sizeof(ether_addr));

  // Only fields required are: sll_family, sll_protocol, and sll_ifindex;
  ether_addr.sll_family = AF_PACKET;
  ether_addr.sll_protocol = htons(ETHERTYPE_IPV6);
  ether_addr.sll_ifindex = if_index;

  if (bind(ether_fd, reinterpret_cast<sockaddr*>(&ether_addr),
           sizeof(ether_addr)) < 0) {
    const int saved_errno = errno;
    CloseFd();
    if (EACCES == saved_errno) {
      return Status(Code::BAD_PERMISSIONS)
             << "Process does not have permission to bind to interface";
    }
    if (EADDRINUSE == saved_errno) {
      return Status(Code::RESOURCE_IN_USE)
             << "Interface " << name() << " is already bound to another socket";
    }
    return Status(Code::UNEXPECTED_FAILURE)
           << "bind(): " << safe_strerror(saved_errno);
  }
  state_ = State::READY;

  return Status();
}

EtherSocket::~EtherSocket() {
  if (IsReady()) {
    Close();
  } else if (IsUnitialized() && fd() != -1) {
    CloseFd();
    state_ = State::CLOSED;
  }
}

Status EtherSocket::AttachFilter(const struct sock_fprog* sock_filter_prog) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  if (sock_filter_prog == nullptr) {
    // Remove socket filter.
    if (setsockopt(fd(), SOL_SOCKET, SO_DETACH_FILTER, nullptr, 0) < 0) {
      const int saved_errno = errno;
      return Status(Code::UNEXPECTED_FAILURE)
             << "Failed to detach BPF: setsockopt(): "
             << safe_strerror(saved_errno);
    }
  } else {
    // Attach socket filter.
    if (setsockopt(fd(), SOL_SOCKET, SO_ATTACH_FILTER, sock_filter_prog,
                   sizeof(struct sock_fprog)) < 0) {
      const int saved_errno = errno;
      return Status(Code::UNEXPECTED_FAILURE)
             << "Failed to attach BPF: setsockopt(): "
             << safe_strerror(saved_errno);
    }
  }
  return Status();
}

Status EtherSocket::SetAllMulticastMode(bool enabled) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  int16_t flags;
  PORTIER_RETURN_ON_FAILURE(GetInterfaceFlags(&flags));

  if (enabled) {
    SetBits(&flags, IFF_ALLMULTI);
  } else {
    ClearBits(&flags, IFF_ALLMULTI);
  }
  PORTIER_RETURN_ON_FAILURE(SetInterfaceFlags(flags))
      << "Failed to set all-multicast mode";
  return Status();
}

Status EtherSocket::SetPromiscuousMode(bool enabled) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  int16_t flags;
  PORTIER_RETURN_ON_FAILURE(GetInterfaceFlags(&flags));

  if (enabled) {
    SetBits(&flags, IFF_PROMISC);
  } else {
    ClearBits(&flags, IFF_PROMISC);
  }
  PORTIER_RETURN_ON_FAILURE(SetInterfaceFlags(flags))
      << "Failed to set all-multicast mode";
  return Status();
}

Status EtherSocket::ReceiveIPv6Packet(IPv6EtherHeader* header_fields,
                                      ByteString* payload) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  uint8_t buffer[kReceiveBufferSize];
  const int32_t res = HANDLE_EINTR(recv(fd(), buffer, kReceiveBufferSize, 0));

  if (res < 0) {
    const int saved_errno = errno;
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to receive packet: recv(): "
           << safe_strerror(saved_errno);
  }

  if (res == 0) {
    return Status(Code::RESULT_UNAVAILABLE, "Nothing was received");
  }

  if (res < (sizeof(struct ether_header) + sizeof(struct ip6_hdr))) {
    return Status(Code::MALFORMED_PACKET) << base::StringPrintf(
               "Packet length is smaller than expected: received %d bytes",
               res);
  }

  // Theoretically possible to receive a packet larger than 1500 bytes
  // (ethernet MTU) on a proprietary networks, such as link-layer
  // networks that support jumbograms.  The buffer would have been
  // filled only upto |kReceiveBufferSize| bytes, but the data is
  // truncated.
  if (res > kReceiveBufferSize) {
    return Status(Code::UNEXPECTED_FAILURE) << base::StringPrintf(
               "Received packet is larger than internal buffers: Actual size "
               "%u bytes",
               res);
  }

  const uint8_t* const ether_ptr = buffer;
  const struct ether_header* ether_hdr =
      reinterpret_cast<const struct ether_header*>(ether_ptr);

  if (ntohs(ether_hdr->ether_type) != ETHERTYPE_IPV6) {
    return Status(Code::MALFORMED_PACKET) << base::StringPrintf(
               "Ether type is not IPv6: %hx", ntohs(ether_hdr->ether_type));
  }

  const uint8_t* const ip6_ptr = &ether_ptr[sizeof(struct ether_header)];
  const struct ip6_hdr* ip6_hdr =
      reinterpret_cast<const struct ip6_hdr*>(ip6_ptr);

  // Check that the IP version (upper 4-bits of the first oclet) is 6.
  if ((ip6_hdr->ip6_vfc & kIPVersionMask) != kIPv6VersionBits) {
    return Status(Code::MALFORMED_PACKET)
           << base::StringPrintf("IP version in packet is not IPv6, got %d",
                                 static_cast<int>(ip6_hdr->ip6_vfc >> 4));
  }

  const uint8_t* const payload_ptr = &ip6_ptr[sizeof(struct ip6_hdr)];
  const uint16_t payload_len = ntohs(ip6_hdr->ip6_plen);
  const uint16_t received_payload_len =
      res - sizeof(struct ether_header) - sizeof(struct ip6_hdr);

  if (payload_len != received_payload_len) {
    return Status(Code::MALFORMED_PACKET) << base::StringPrintf(
               "Packet length in IP header (%hu) does not "
               "match the actual length (%hu)",
               payload_len, received_payload_len);
  }

  // Done verification.  Start returning results.

  if (header_fields != nullptr) {
    // Ether frame fields.
    header_fields->destination_ll_address =
        LLAddress(LLAddress::Type::kEui48,
                  ByteString(ether_hdr->ether_dhost, ETHER_ADDR_LEN));
    header_fields->source_ll_address =
        LLAddress(LLAddress::Type::kEui48,
                  ByteString(ether_hdr->ether_shost, ETHER_ADDR_LEN));

    // IPv6 header fields.
    header_fields->ip6_header_flow = ip6_hdr->ip6_flow;
    header_fields->next_header = ip6_hdr->ip6_nxt;
    header_fields->hop_limit = ip6_hdr->ip6_hops;
    header_fields->source_address = IPAddress(
        IPAddress::kFamilyIPv6,
        ByteString(ip6_hdr->ip6_src.s6_addr, sizeof(ip6_hdr->ip6_src.s6_addr)));
    header_fields->destination_address = IPAddress(
        IPAddress::kFamilyIPv6,
        ByteString(ip6_hdr->ip6_dst.s6_addr, sizeof(ip6_hdr->ip6_dst.s6_addr)));
  }

  if (payload != nullptr) {
    if (payload_len == 0) {
      payload->Clear();
    } else {
      *payload = ByteString(payload_ptr, payload_len);
    }
  }

  return Status();
}

Status EtherSocket::DiscardPacket() {
  uint8_t buffer[kReceiveBufferSize];
  const int32_t res = HANDLE_EINTR(recv(fd(), buffer, kReceiveBufferSize, 0));
  if (res < 0) {
    const int saved_errno = errno;
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to receive packet: recv(): "
           << safe_strerror(saved_errno);
  }
  return Status();
}

Status EtherSocket::SendIPv6Packet(const IPv6EtherHeader& header_fields,
                                   const shill::ByteString& payload) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }
  if (header_fields.destination_ll_address.type() != LLAddress::Type::kEui48 ||
      header_fields.source_ll_address.type() != LLAddress::Type::kEui48) {
    return Status(Code::INVALID_ARGUMENT)
           << "Source and destination link-layer addresses must be EUI-48";
  }
  if (header_fields.source_address.family() != IPAddress::kFamilyIPv6 ||
      header_fields.destination_address.family() != IPAddress::kFamilyIPv6) {
    return Status(Code::INVALID_ARGUMENT)
           << "Source and destination IP addresses must be IPv6";
  }

  struct msghdr message_header = {};
  // Ether header + IP header + message (optional)
  struct iovec message_parts[3];
  message_header.msg_iov = message_parts;
  message_header.msg_iovlen = 2;

  // Construct ethernet header.
  struct ether_header ether_hdr = {};

  // Destination and source link-layer address.
  memcpy(ether_hdr.ether_dhost,
         header_fields.destination_ll_address.GetConstData(), ETHER_ADDR_LEN);
  memcpy(ether_hdr.ether_shost, header_fields.source_ll_address.GetConstData(),
         ETHER_ADDR_LEN);
  // Ethernet type to IPv6.
  ether_hdr.ether_type = htons(ETHERTYPE_IPV6);
  // Add to message.
  message_parts[0].iov_base = &ether_hdr;
  message_parts[0].iov_len = sizeof(struct ether_header);

  // Construct IPv6 header.
  struct ip6_hdr ip6_hdr = {};
  // IPv6 flow control.
  ip6_hdr.ip6_flow = header_fields.ip6_header_flow;
  // Force the IP version field to be version 6.  The ip6_vfc
  // attribute is unioned with the ip6_flow.  This operation preserves
  // the upper 4-bits of "Traffic Class" (lower 4-bits of the first
  // oclet).
  ip6_hdr.ip6_vfc = (ip6_hdr.ip6_vfc & ~kIPVersionMask) | kIPv6VersionBits;
  // Payload length.
  ip6_hdr.ip6_plen = htons(payload.GetLength());
  ip6_hdr.ip6_nxt = header_fields.next_header;
  ip6_hdr.ip6_hops = header_fields.hop_limit;
  // Source and destination IPv6 address.
  memcpy(&ip6_hdr.ip6_src, header_fields.source_address.GetConstData(),
         header_fields.source_address.GetLength());
  memcpy(&ip6_hdr.ip6_dst, header_fields.destination_address.GetConstData(),
         header_fields.destination_address.GetLength());
  // Add to message.
  message_parts[1].iov_base = &ip6_hdr;
  message_parts[1].iov_len = sizeof(struct ip6_hdr);

  // Append payload.
  if (payload.GetLength() > 0) {
    message_parts[2].iov_base = const_cast<uint8_t*>(payload.GetConstData());
    message_parts[2].iov_len = payload.GetLength();
    message_header.msg_iovlen++;
  }

  // Prepare socket address for sendto().
  struct sockaddr_ll addr;
  memset(&addr, 0, sizeof(struct sockaddr_ll));
  addr.sll_ifindex = index();
  addr.sll_halen = ETHER_ADDR_LEN;
  memcpy(addr.sll_addr, header_fields.destination_ll_address.GetConstData(),
         ETHER_ADDR_LEN);

  const int32_t res = HANDLE_EINTR(sendmsg(fd(), &message_header, 0));
  if (res < 0) {
    const int saved_errno = errno;
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to send IPv6 ether packet: sendto(): "
           << safe_strerror(saved_errno);
  }

  return Status();
}

}  // namespace portier
