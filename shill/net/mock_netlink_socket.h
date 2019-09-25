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

  uint32_t GetLastSequenceNumber() const { return sequence_number_; }

  MOCK_METHOD(bool, Init, (), (override));
  MOCK_METHOD(int, file_descriptor, (), (const, override));
  MOCK_METHOD(bool, SendMessage, (const ByteString&), (override));
  MOCK_METHOD(bool, SubscribeToEvents, (uint32_t), (override));
  MOCK_METHOD(bool, RecvMessage, (ByteString*), (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetlinkSocket);
};

}  // namespace shill

#endif  // SHILL_NET_MOCK_NETLINK_SOCKET_H_
