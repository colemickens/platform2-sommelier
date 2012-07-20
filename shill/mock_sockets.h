// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_SOCKETS_H_
#define SHILL_MOCK_SOCKETS_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/sockets.h"

namespace shill {

class MockSockets : public Sockets {
 public:
  MockSockets();
  virtual ~MockSockets();

  MOCK_METHOD3(Accept,
               int(int sockfd, struct sockaddr *addr, socklen_t *addrlen));
  MOCK_METHOD2(AttachFilter, int(int sockfd, struct sock_fprog *pf));
  MOCK_METHOD3(Bind,
               int(int sockfd, const struct sockaddr *addr, socklen_t addrlen));
  MOCK_METHOD2(BindToDevice, int(int sockfd, const std::string &device));
  MOCK_METHOD1(Close, int(int fd));
  MOCK_METHOD3(Connect,
               int(int sockfd, const struct sockaddr *addr, socklen_t addrlen));
  MOCK_METHOD0(Error, int());
  MOCK_METHOD3(GetSockName,
               int(int sockfd, struct sockaddr *addr, socklen_t *addrlen));
  MOCK_METHOD1(GetSocketError, int(int fd));
  MOCK_METHOD3(Ioctl, int(int d, int request, void *argp));
  MOCK_METHOD2(Listen, int(int d, int backlog));
  MOCK_METHOD6(RecvFrom, ssize_t(int sockfd,
                                 void *buf,
                                 size_t len,
                                 int flags,
                                 struct sockaddr *src_addr,
                                 socklen_t *addrlen));
  MOCK_METHOD4(Send,
               ssize_t(int sockfd, const void *buf, size_t len, int flags));
  MOCK_METHOD6(SendTo, ssize_t(int sockfd,
                               const void *buf,
                               size_t len,
                               int flags,
                               const struct sockaddr *dest_addr,
                               socklen_t addrlen));
  MOCK_METHOD1(SetNonBlocking, int(int sockfd));
  MOCK_METHOD3(Socket, int(int domain, int type, int protocol));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSockets);
};

}  // namespace shill

#endif  // SHILL_MOCK_SOCKETS_H_
