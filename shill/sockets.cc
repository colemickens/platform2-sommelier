// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/sockets.h"

#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <base/eintr_wrapper.h>

#include "shill/logging.h"

namespace shill {

Sockets::~Sockets() {}

// Some system calls can be interrupted and return EINTR, but will succeed on
// retry.  The HANDLE_EINTR macro retries a call if it returns EINTR.  For a
// list of system calls that can return EINTR, see 'man 7 signal' under the
// heading "Interruption of System Calls and Library Functions by Signal
// Handlers".

int Sockets::Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  return HANDLE_EINTR(accept(sockfd, addr, addrlen));
}

int Sockets::AttachFilter(int sockfd, struct sock_fprog *pf) {
  return setsockopt(sockfd, SOL_SOCKET, SO_ATTACH_FILTER, pf, sizeof(*pf));
}

int Sockets::Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  return bind(sockfd, addr, addrlen);
}

int Sockets::BindToDevice(int sockfd, const std::string &device) {
  char dev_name[IFNAMSIZ];
  CHECK_GT(sizeof(dev_name), device.length());
  memset(&dev_name, 0, sizeof(dev_name));
  snprintf(dev_name, sizeof(dev_name), "%s", device.c_str());
  return HANDLE_EINTR(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, &dev_name,
                                 sizeof(dev_name)));
}

int Sockets::Close(int fd) {
  return HANDLE_EINTR(close(fd));
}

int Sockets::Connect(int sockfd, const struct sockaddr *addr,
                     socklen_t addrlen) {
  return HANDLE_EINTR(connect(sockfd, addr, addrlen));
}

int Sockets::Error() {
  return errno;
}

std::string Sockets::ErrorString() {
  return std::string(strerror(Error()));
}

int Sockets::GetSockName(int sockfd,
                         struct sockaddr *addr,
                         socklen_t *addrlen) {
  return getsockname(sockfd, addr, addrlen);
}


int Sockets::GetSocketError(int sockfd) {
  int error;
  socklen_t optlen = sizeof(error);
  if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &optlen) == 0) {
    return error;
  }
  return -1;
}


int Sockets::Ioctl(int d, int request, void *argp) {
  return HANDLE_EINTR(ioctl(d, request, argp));
}

int Sockets::Listen(int sockfd, int backlog) {
  return listen(sockfd, backlog);
}

ssize_t Sockets::RecvFrom(int sockfd, void *buf, size_t len, int flags,
                          struct sockaddr *src_addr, socklen_t *addrlen) {
  return HANDLE_EINTR(recvfrom(sockfd, buf, len, flags, src_addr, addrlen));
}

ssize_t Sockets::Send(int sockfd, const void *buf, size_t len, int flags) {
  return HANDLE_EINTR(send(sockfd, buf, len, flags));
}

ssize_t Sockets::SendTo(int sockfd, const void *buf, size_t len, int flags,
                        const struct sockaddr *dest_addr, socklen_t addrlen) {
  return HANDLE_EINTR(sendto(sockfd, buf, len, flags, dest_addr, addrlen));
}

int Sockets::SetNonBlocking(int sockfd) {
  return HANDLE_EINTR(
      fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK));
}

int Sockets::Socket(int domain, int type, int protocol) {
  return socket(domain, type, protocol);
}

ScopedSocketCloser::ScopedSocketCloser(Sockets *sockets, int fd)
    : sockets_(sockets),
      fd_(fd) {}

ScopedSocketCloser::~ScopedSocketCloser() {
  sockets_->Close(fd_);
  fd_ = -1;
}

}  // namespace shill
