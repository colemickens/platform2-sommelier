// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_RTNL_HANDLER_H_
#define SHILL_MOCK_RTNL_HANDLER_H_

#include <gmock/gmock.h>

#include "shill/rtnl_handler.h"

namespace shill {

class MockRTNLHandler : public RTNLHandler {
 public:
  MOCK_METHOD2(Start, void(EventDispatcher *dispatcher, Sockets *sockets));
  MOCK_METHOD1(AddListener, void(RTNLListener *to_add));
  MOCK_METHOD1(RemoveListener, void(RTNLListener *to_remove));
  MOCK_METHOD3(SetInterfaceFlags, void(int interface_index,
                                       unsigned int flags,
                                       unsigned int change));
  MOCK_METHOD2(AddInterfaceAddress, bool(int interface_index,
                                         const IPConfig &config));
  MOCK_METHOD2(RemoveInterfaceAddress, bool(int interface_index,
                                            const IPConfig &config));
  MOCK_METHOD1(RequestDump, void(int request_flags));
  MOCK_METHOD1(GetInterfaceIndex, int(const std::string &interface_name));
  MOCK_METHOD1(SendMessage, bool(RTNLMessage *message));
};

}  // namespace shill

#endif  // SHILL_MOCK_RTNL_HANDLER_H_
