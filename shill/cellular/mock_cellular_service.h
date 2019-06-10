// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_CELLULAR_SERVICE_H_
#define SHILL_CELLULAR_MOCK_CELLULAR_SERVICE_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/cellular/cellular_service.h"

namespace shill {

class MockCellularService : public CellularService {
 public:
  MockCellularService(Manager* manager, const CellularRefPtr& device);
  ~MockCellularService() override;

  MOCK_METHOD0(AutoConnect, void());
  MOCK_METHOD1(SetLastGoodApn, void(const Stringmap& apn_info));
  MOCK_METHOD0(ClearLastGoodApn, void());
  MOCK_METHOD1(SetActivationState, void(const std::string& state));
  MOCK_METHOD2(Connect, void(Error* error, const char* reason));
  MOCK_METHOD2(Disconnect, void(Error* error, const char* reason));
  MOCK_METHOD1(SetState, void(ConnectState state));
  MOCK_METHOD1(SetFailure, void(ConnectFailure failure));
  MOCK_METHOD1(SetFailureSilent, void(ConnectFailure failure));
  MOCK_CONST_METHOD0(state, ConnectState());
  MOCK_CONST_METHOD0(explicitly_disconnected, bool());
  MOCK_CONST_METHOD0(activation_state, const std::string&());

 private:
  std::string default_activation_state_;

  DISALLOW_COPY_AND_ASSIGN(MockCellularService);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_CELLULAR_SERVICE_H_
