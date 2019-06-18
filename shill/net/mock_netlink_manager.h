// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_MOCK_NETLINK_MANAGER_H_
#define SHILL_NET_MOCK_NETLINK_MANAGER_H_

#include "shill/net/netlink_manager.h"

#include <string>

#include <gmock/gmock.h>

namespace shill {

class MockNetlinkManager : public NetlinkManager {
 public:
  MockNetlinkManager() = default;
  ~MockNetlinkManager() override = default;

  MOCK_METHOD0(Init, bool());
  MOCK_METHOD0(Start, void());
  MOCK_METHOD2(
      GetFamily,
      uint16_t(const std::string& family_name,
               const NetlinkMessageFactory::FactoryMethod& message_factory));
  MOCK_METHOD1(RemoveBroadcastHandler,
               bool(const NetlinkMessageHandler& message_handler));
  MOCK_METHOD1(AddBroadcastHandler,
               bool(const NetlinkMessageHandler& messge_handler));
  MOCK_METHOD4(SendControlMessage,
               bool(ControlNetlinkMessage* message,
                    const ControlNetlinkMessageHandler& message_handler,
                    const NetlinkAckHandler& ack_handler,
                    const NetlinkAuxilliaryMessageHandler& error_handler));
  MOCK_METHOD4(SendNl80211Message,
               bool(Nl80211Message* message,
                    const Nl80211MessageHandler& message_handler,
                    const NetlinkAckHandler& ack_handler,
                    const NetlinkAuxilliaryMessageHandler& error_handler));
  MOCK_METHOD2(SubscribeToEvents,
               bool(const std::string& family, const std::string& group));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetlinkManager);
};

}  // namespace shill

#endif  // SHILL_NET_MOCK_NETLINK_MANAGER_H_
