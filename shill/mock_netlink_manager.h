// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_NETLINK_MANAGER_H_
#define SHILL_MOCK_NETLINK_MANAGER_H_

#include "shill/netlink_manager.h"

#include <map>

#include <gmock/gmock.h>

namespace shill {

class NetlinkMessage;

class MockNetlinkManager : public NetlinkManager {
 public:
  MockNetlinkManager();
  virtual ~MockNetlinkManager();
  MOCK_METHOD1(RemoveBroadcastHandler,
               bool(const NetlinkMessageHandler &message_handler));
  MOCK_METHOD1(AddBroadcastHandler,
               bool(const NetlinkMessageHandler &messge_handler));
  MOCK_METHOD2(SendMessage,
               bool(NetlinkMessage *message,
                    const NetlinkMessageHandler &message_handler));
  MOCK_METHOD2(SubscribeToEvents,
               bool(const std::string &family, const std::string &group));
  MOCK_CONST_METHOD1(GetMessageType, uint16_t(const std::string &name));

  void SetMessageType(std::string name, uint16_t type);

 private:
  uint16_t DoGetMessageType(std::string name);

  std::map<std::string, uint16_t> message_types_;

  DISALLOW_COPY_AND_ASSIGN(MockNetlinkManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_NETLINK_MANAGER_H_
