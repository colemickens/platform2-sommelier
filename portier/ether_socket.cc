// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <arpa/inet.h>
#include <errno.h>
#include <net/ethernet.h>  // L2 stuff.
#include <net/if.h>        // Interface stuff.
#include <netinet/ip6.h>   // IPv6 stuff.
#include <sys/socket.h>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/posix/safe_strerror.h>
#include <base/strings/stringprintf.h>

#include "portier/ether_socket.h"

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
  fd_ = ether_fd;

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
    if (setsockopt(fd(), SOL_SOCKET, SO_DETACH_FILTER, NULL, 0) < 0) {
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
  LOG(FATAL) << "Not implemented: EtherSocket::ReceiveIPv6Packet()";
  return Status();
}

Status EtherSocket::DiscardPacket() {
  LOG(FATAL) << "Not implemented: EtherSocket::DiscardPacket()";
  return Status();
}

Status EtherSocket::SendIPv6Packet(const IPv6EtherHeader& header_fields,
                                   const shill::ByteString& payload) {
  LOG(FATAL) << "Not implemented: EtherSocket::SendIPv6Packet()";
  return Status();
}

}  // namespace portier
