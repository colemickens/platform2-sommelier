// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_MOCK_NETLINK_SOCKET_H_
#define SHILL_NET_MOCK_NETLINK_SOCKET_H_

#include "shill/net/netlink_socket.h"

#include <base/macros.h>

#include <gmock/gmock.h>

namespace shill {

class ByteString;

class MockNetlinkSocket : public NetlinkSocket {
 public:
  MockNetlinkSocket() = default;
  MOCK_METHOD0(Init, bool());

  uint32_t GetLastSequenceNumber() const { return sequence_number_; }
  MOCK_CONST_METHOD0(file_descriptor, int());
  MOCK_METHOD1(SendMessage, bool(const ByteString& out_string));
  MOCK_METHOD1(SubscribeToEvents, bool(uint32_t group_id));
  MOCK_METHOD1(RecvMessage, bool(ByteString* message));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetlinkSocket);
};

}  // namespace shill

#endif  // SHILL_NET_MOCK_NETLINK_SOCKET_H_
