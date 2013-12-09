// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/icmp.h"

#include <netinet/in.h>
#include <netinet/ip_icmp.h>

#include "shill/ip_address.h"
#include "shill/logging.h"
#include "shill/sockets.h"

namespace shill {

Icmp::Icmp()
    : sockets_(new Sockets()),
      socket_(-1) {}

Icmp::~Icmp() {}

bool Icmp::Start() {
  int socket = sockets_->Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (socket == -1) {
    PLOG(ERROR) << "Could not create ICMP socket";
    Stop();
    return false;
  }
  socket_ = socket;
  socket_closer_.reset(new ScopedSocketCloser(sockets_.get(), socket_));

  if (sockets_->SetNonBlocking(socket_) != 0) {
    PLOG(ERROR) << "Could not set socket to be non-blocking";
    Stop();
    return false;
  }

  return true;
}

void Icmp::Stop() {
  socket_closer_.reset();
  socket_ = -1;
}

bool Icmp::IsStarted() const {
  return socket_closer_.get();
}

bool Icmp::TransmitEchoRequest(const IPAddress &destination) {
  if (!IsStarted() && !Start()) {
    return false;
  }

  if (!destination.IsValid()) {
    LOG(ERROR) << "Destination address is not valid.";
    return false;
  }

  if (destination.family() != IPAddress::kFamilyIPv4) {
    NOTIMPLEMENTED() << "Only IPv4 destination addresses are implemented.";
    return false;
  }

  struct icmphdr icmp_header;
  memset(&icmp_header, 0, sizeof(icmp_header));
  icmp_header.type = ICMP_ECHO;
  icmp_header.un.echo.id = 1;
  icmp_header.un.echo.sequence = 1;

  struct sockaddr_in destination_address;
  destination_address.sin_family = AF_INET;
  CHECK_EQ(sizeof(destination_address.sin_addr.s_addr),
           destination.GetLength());
  memcpy(&destination_address.sin_addr.s_addr,
         destination.address().GetConstData(),
         sizeof(destination_address.sin_addr.s_addr));

  int result = sockets_->SendTo(
      socket_,
      &icmp_header,
      sizeof(icmp_header),
      0,
      reinterpret_cast<struct sockaddr *>(&destination_address),
      sizeof(destination_address));
  int expected_result = sizeof(icmp_header);
  if (result != expected_result) {
    if (result < 0) {
      PLOG(ERROR) << "Socket sendto failed";
    } else if (result < expected_result) {
      LOG(ERROR) << "Socket sendto returned "
                 << result
                 << " which is less than the expected result "
                 << expected_result;
    }
    return false;
  }

  return true;
}

}  // namespace shill
