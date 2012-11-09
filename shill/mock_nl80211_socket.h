// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_NL80211_SOCKET_
#define SHILL_MOCK_NL80211_SOCKET_

#include <gmock/gmock.h>

#include <netlink/attr.h>
#include <netlink/netlink.h>

#include <string>

#include "shill/nl80211_socket.h"


namespace shill {

class MockNl80211Socket : public Nl80211Socket {
 public:
  MockNl80211Socket() : sequence_number_(1) {}
  MOCK_METHOD0(Init, bool());
  MOCK_METHOD1(AddGroupMembership, bool(const std::string &group_name));
  using Nl80211Socket::GetMessages;
  MOCK_METHOD0(GetMessages, bool());
  using Nl80211Socket::SetNetlinkCallback;
  MOCK_METHOD2(SetNetlinkCallback, bool(nl_recvmsg_msg_cb_t on_netlink_data,
                                        void *callback_parameter));
  MOCK_METHOD0(GetSequenceNumber, unsigned int());

  virtual uint32 Send(KernelBoundNlMessage *message);

  uint32 GetLastSequenceNumber() const { return sequence_number_; }

 private:
  uint32 sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(MockNl80211Socket);
};

}  // namespace shill

#endif  // SHILL_MOCK_NL80211_SOCKET_
