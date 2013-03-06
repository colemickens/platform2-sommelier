// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_NETLINK_SOCKET_H_
#define SHILL_MOCK_NETLINK_SOCKET_H_

#include <netlink/attr.h>
#include <netlink/netlink.h>

#include <string>

#include <gmock/gmock.h>

#include "shill/netlink_socket.h"


namespace shill {

class ByteString;

class MockNetlinkSocket : public NetlinkSocket {
 public:
  MockNetlinkSocket() {}
  MOCK_METHOD0(Init, bool());

  virtual bool SendMessage(const ByteString &out_string);
  uint32 GetLastSequenceNumber() const { return sequence_number_; }
  MOCK_METHOD1(SubscribeToEvents, bool(uint32_t group_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetlinkSocket);
};

}  // namespace shill

#endif  // SHILL_MOCK_NETLINK_SOCKET_H_
