// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_SOCKETS_H_
#define SHILL_MOCK_SOCKETS_H_

#include <gmock/gmock.h>

#include "shill/sockets.h"

namespace shill {

class MockSockets : public Sockets {
 public:
  MOCK_METHOD3(Bind,
               int(int sockfd, const struct sockaddr *addr, socklen_t addrlen));
  MOCK_METHOD1(Close, int(int fd));
  MOCK_METHOD4(Send,
               ssize_t(int sockfd, const void *buf, size_t len, int flags));
  MOCK_METHOD6(SendTo, ssize_t(int sockfd,
                               const void *buf,
                               size_t len,
                               int flags,
                               const struct sockaddr *dest_addr,
                               socklen_t addrlen));
  MOCK_METHOD3(Socket, int(int domain, int type, int protocol));
};

}  // namespace shill

#endif  // SHILL_MOCK_SOCKETS_H_
