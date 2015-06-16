// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_MOCK_WIMAX_H_
#define SHILL_WIMAX_MOCK_WIMAX_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/wimax/wimax.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;

class MockWiMax : public WiMax {
 public:
  MockWiMax(ControlInterface* control,
            EventDispatcher* dispatcher,
            Metrics* metrics,
            Manager* manager,
            const std::string& link_name,
            const std::string& address,
            int interface_index,
            const RpcIdentifier& path);
  ~MockWiMax() override;

  MOCK_METHOD2(Start, void(Error* error,
                           const EnabledStateChangedCallback& callback));
  MOCK_METHOD2(Stop, void(Error* error,
                          const EnabledStateChangedCallback& callback));
  MOCK_METHOD2(ConnectTo, void(const WiMaxServiceRefPtr& service,
                               Error* error));
  MOCK_METHOD2(DisconnectFrom, void(const ServiceRefPtr& service,
                                    Error* error));
  MOCK_CONST_METHOD0(IsIdle, bool());
  MOCK_METHOD1(OnServiceStopped, void(const WiMaxServiceRefPtr& service));
  MOCK_METHOD0(OnDeviceVanished, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiMax);
};

}  // namespace shill

#endif  // SHILL_WIMAX_MOCK_WIMAX_H_
