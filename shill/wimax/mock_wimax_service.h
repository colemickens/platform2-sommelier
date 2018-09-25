// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_MOCK_WIMAX_SERVICE_H_
#define SHILL_WIMAX_MOCK_WIMAX_SERVICE_H_

#include <memory>

#include <gmock/gmock.h>

#include "shill/wimax/wimax_service.h"

namespace shill {

class MockWiMaxService : public WiMaxService {
 public:
  MockWiMaxService(ControlInterface* control,
                   EventDispatcher* dispatcher,
                   Metrics* metrics,
                   Manager* manager);
  ~MockWiMaxService() override;

  MOCK_CONST_METHOD0(GetNetworkObjectPath, RpcIdentifier());

  // gmock doesn't support a std::unique_ptr<> argument in MOCK_METHOD, so we
  // use override Start() to forward the call to MockableStart.
  bool Start(std::unique_ptr<WiMaxNetworkProxyInterface> proxy) override;
  MOCK_METHOD1(MockableStart, bool(WiMaxNetworkProxyInterface* proxy));

  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(IsStarted, bool());
  MOCK_METHOD1(SetState, void(ConnectState state));
  MOCK_METHOD0(ClearPassphrase, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiMaxService);
};

}  // namespace shill

#endif  // SHILL_WIMAX_MOCK_WIMAX_SERVICE_H_
