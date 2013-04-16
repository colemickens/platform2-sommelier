// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_NETLINK_MANAGER_H_
#define SHILL_MOCK_NETLINK_MANAGER_H_

#include <gmock/gmock.h>

#include "shill/netlink_manager.h"

namespace shill {

class MockNetlinkManager : public NetlinkManager {
 public:
  MockNetlinkManager() {}
  ~MockNetlinkManager() {}

  MOCK_METHOD2(SendMessage, bool(NetlinkMessage *message,
                                 const NetlinkMessageHandler &message_handler));
};

}  // namespace shill

#endif  // SHILL_MOCK_NETLINK_MANAGER_H_
