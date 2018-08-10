// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/network_socket.h"

#include <errno.h>
#include <net/ethernet.h>  // L2 stuff.
#include <net/if_arp.h>    // ARPHRD_* definitions.
#include <string.h>
#include <sys/ioctl.h>

#include <base/logging.h>
#include <base/posix/safe_strerror.h>
#include <base/strings/stringprintf.h>
#include <shill/net/byte_string.h>

namespace portier {

using std::string;
using base::safe_strerror;
using shill::ByteString;

using Code = Status::Code;

using State = NetworkSocket::State;

namespace {

// Used for ioctl() calls.
constexpr int kTrue = true;
constexpr int kFalse = false;

}  // namespace

string NetworkSocket::GetStateName(State state) {
  switch (state) {
    case State::UNINITIALIZED:
      return "Unitialized";
    case State::READY:
      return "Ready";
    case State::CLOSED:
      return "Closed";
  }
  return base::StringPrintf("Unknown (%d)", static_cast<int>(state));
}

NetworkSocket::NetworkSocket(const string& if_name)
    : name_(if_name), index_(-1), state_(State::UNINITIALIZED) {}

// State getters.

// private.
bool NetworkSocket::IsUnitialized() const {
  return state() == State::UNINITIALIZED;
}

bool NetworkSocket::IsReady() const {
  return state() == State::READY;
}

bool NetworkSocket::IsClosed() const {
  return state() == State::CLOSED;
}

Status NetworkSocket::Close() {
  if (IsUnitialized()) {
    return Status(Code::BAD_INTERNAL_STATE,
                  "Cannot close an uninitialized socket");
  }
  if (IsClosed()) {
    return Status();
  }

  CloseFd();
  state_ = State::CLOSED;

  return Status();
}

void NetworkSocket::CloseFd() {
  fd_.reset();
}

// Interface information getters.

Status NetworkSocket::GetLinkLayerAddress(LLAddress* ll_address) const {
  DCHECK(ll_address != nullptr)
      << "Output parameter `ll_address' cannot be null";
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  struct ifreq ifreq;
  PrepareIfRequestStruct(&ifreq);
  if (ioctl(fd(), SIOCGIFHWADDR, &ifreq) < 0) {
    const int saved_errno = errno;
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to get interface hardware address: ioctl(): "
           << safe_strerror(saved_errno);
  }

  // TODO(sigquit): figure out how to validate the device type
  //  (ifreq.ifr_hwaddr.sa_family) is supported by the current framework.
  //  A list of possible values can be found in <linux/if_arp.h>, using the
  //  ARPHRD_* list.  Good luck!
  //  For now, going to just assume the only valid option is ARPHRD_ETHER.
  if (ARPHRD_ETHER != ifreq.ifr_hwaddr.sa_family) {
    return Status(Code::UNSUPPORTED_TYPE)
           << "Hardware type is not supported: "
           << base::StringPrintf("Got ARPHRD id %d",
                                 ifreq.ifr_hwaddr.sa_family);
  }

  *ll_address = LLAddress(LLAddress::Type::kEui48,
                          ByteString(ifreq.ifr_hwaddr.sa_data, ETHER_ADDR_LEN));

  return Status();
}

Status NetworkSocket::GetLinkMTU(uint32_t* mtu) const {
  DCHECK(mtu != nullptr) << "Output parameter `mtu' cannot be null";
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  struct ifreq ifreq;
  PrepareIfRequestStruct(&ifreq);
  if (ioctl(fd(), SIOCGIFMTU, &ifreq) < 0) {
    const int saved_errno = errno;
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to get interface MTU: ioctl(): "
           << safe_strerror(saved_errno);
  }

  *mtu = static_cast<uint32_t>(ifreq.ifr_mtu);
  return Status();
}

Status NetworkSocket::GetLoopbackFlag(bool* loopback_flag) const {
  DCHECK(loopback_flag != nullptr)
      << "Output parameter `loopback_flag' cannot be null";
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  int16_t flags = 0;
  PORTIER_RETURN_ON_FAILURE(GetInterfaceFlags(&flags))
      << "Failed to get loopback flag";

  *loopback_flag = ((flags & IFF_LOOPBACK) != 0);
  return Status();
}

Status NetworkSocket::SetNonBlockingMode(bool enabled) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  if (ioctl(fd(), FIONBIO, enabled ? &kTrue : &kFalse) < 0) {
    const int saved_errno = errno;
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to set non-blocking mode enabled "
           << (enabled ? "true" : "false")
           << ": setsockopt(): " << safe_strerror(saved_errno);
  }
  return Status();
}

// private.
void NetworkSocket::PrepareIfRequestStruct(struct ifreq* ifreq) const {
  DCHECK(ifreq != nullptr);
  memset(ifreq, 0, sizeof(struct ifreq));
  strncpy(ifreq->ifr_name, name().c_str(), IFNAMSIZ - 1);
}

// private.
Status NetworkSocket::SetInterfaceFlags(int16_t flags) {
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  struct ifreq ifreq;
  PrepareIfRequestStruct(&ifreq);
  ifreq.ifr_flags = flags;
  if (ioctl(fd(), SIOCSIFFLAGS, &ifreq) < 0) {
    const int saved_errno = errno;
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to set interface flags: ioctl(): "
           << safe_strerror(saved_errno);
  }

  return Status();
}

// private.
Status NetworkSocket::GetInterfaceFlags(int16_t* flags) const {
  DCHECK(flags != nullptr) << "Output parameter `flags' cannot be null";
  if (!IsReady()) {
    return Status(Code::BAD_INTERNAL_STATE) << "Socket is not ready";
  }

  struct ifreq ifreq;
  PrepareIfRequestStruct(&ifreq);
  if (ioctl(fd(), SIOCGIFFLAGS, &ifreq) < 0) {
    const int saved_errno = errno;
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to get interface flags: ioctl(): "
           << safe_strerror(saved_errno);
  }

  *flags = ifreq.ifr_flags;
  return Status();
}

}  // namespace portier
