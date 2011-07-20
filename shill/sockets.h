// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SOCKETS_H_
#define SHILL_SOCKETS_H_

#include <sys/types.h>

#include <base/basictypes.h>

namespace shill {

// A "sys/socket.h" abstraction allowing mocking in tests.
class Sockets {
 public:
  virtual ~Sockets();

  // bind
  virtual int Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

  // close
  virtual int Close(int fd);

  // ioctl
  virtual int Ioctl(int d, int request, void *argp);

  // send
  virtual ssize_t Send(int sockfd, const void *buf, size_t len, int flags);

  // sendto
  virtual ssize_t SendTo(int sockfd, const void *buf, size_t len, int flags,
                         const struct sockaddr *dest_addr, socklen_t addrlen);

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
