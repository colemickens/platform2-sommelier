// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_MOCK_ETHERNET_H_
#define SHILL_ETHERNET_MOCK_ETHERNET_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/ethernet/ethernet.h"
#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;

class MockEthernet : public Ethernet {
 public:
  MockEthernet(Manager* manager,
               const std::string& link_name,
               const std::string& address,
               int interface_index);
  ~MockEthernet() override;

  MOCK_METHOD2(Start, void(Error* error,
                           const EnabledStateChangedCallback& callback));
  MOCK_METHOD2(Stop, void(Error* error,
                          const EnabledStateChangedCallback& callback));
  MOCK_METHOD1(ConnectTo, void(EthernetService* service));
  MOCK_METHOD1(DisconnectFrom, void(EthernetService* service));
  MOCK_CONST_METHOD0(IsConnectedViaTether, bool());
  MOCK_CONST_METHOD0(link_up, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEthernet);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_MOCK_ETHERNET_H_
