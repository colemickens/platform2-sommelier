// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <unistd.h>

#include "shill/sockets.h"

namespace shill {

Sockets::~Sockets() {}

int Sockets::Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  return bind(sockfd, addr, addrlen);
}

int Sockets::Close(int fd) {
  return close(fd);
}

ssize_t Sockets::Send(int sockfd, const void *buf, size_t len, int flags) {
  return send(sockfd, buf, len, flags);
}

ssize_t Sockets::SendTo(int sockfd, const void *buf, size_t len, int flags,
                        const struct sockaddr *dest_addr, socklen_t addrlen) {
  return sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

int Sockets::Socket(int domain, int type, int protocol) {
  return socket(domain, type, protocol);
}

}  // namespace shill
