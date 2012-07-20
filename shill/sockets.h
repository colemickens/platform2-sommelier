// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SOCKETS_H_
#define SHILL_SOCKETS_H_

#include <string>

#include <linux/filter.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <base/basictypes.h>

namespace shill {

// A "sys/socket.h" abstraction allowing mocking in tests.
class Sockets {
 public:
  virtual ~Sockets();

  // accept
  virtual int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

  // getsockopt(sockfd, SOL_SOCKET, SO_ATTACH_FILTER, ...)
  virtual int AttachFilter(int sockfd, struct sock_fprog *pf);

  // bind
  virtual int Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

  // setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE ...)
  virtual int BindToDevice(int sockfd, const std::string &device);

  // close
  virtual int Close(int fd);

  // connect
  virtual int Connect(int sockfd, const struct sockaddr *addr,
                      socklen_t addrlen);

  // errno
  virtual int Error();

  // errno
  virtual std::string ErrorString();

  // getsockname
  virtual int GetSockName(int sockfd, struct sockaddr *addr,
                          socklen_t *addrlen);

  // getsockopt(sockfd, SOL_SOCKET, SO_ERROR, ...)
  virtual int GetSocketError(int sockfd);

  // ioctl
  virtual int Ioctl(int d, int request, void *argp);

  // listen
  virtual int Listen(int sockfd, int backlog);

  // recvfrom
  virtual ssize_t RecvFrom(int sockfd, void *buf, size_t len, int flags,
                           struct sockaddr *src_addr, socklen_t *addrlen);

  // send
  virtual ssize_t Send(int sockfd, const void *buf, size_t len, int flags);

  // sendto
  virtual ssize_t SendTo(int sockfd, const void *buf, size_t len, int flags,
                         const struct sockaddr *dest_addr, socklen_t addrlen);

  // fcntl(sk, F_SETFL, fcntl(sk, F_GETFL) | O_NONBLOCK)
  virtual int SetNonBlocking(int sockfd);

  // socket
  virtual int Socket(int domain, int type, int protocol);
};

class ScopedSocketCloser {
 public:
  ScopedSocketCloser(Sockets *sockets, int fd);
  ~ScopedSocketCloser();

 private:
  Sockets *sockets_;
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSocketCloser);
};

}  // namespace shill

#endif  // SHILL_SOCKETS_H_
