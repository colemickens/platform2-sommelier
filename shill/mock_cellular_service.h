// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CELLULAR_SERVICE_H_
#define SHILL_MOCK_CELLULAR_SERIVCE_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/cellular_service.h"

namespace shill {

class MockCellularService : public CellularService {
 public:
  MockCellularService(ModemInfo *modem_info,
                      const CellularRefPtr &device);
  virtual ~MockCellularService();

  MOCK_METHOD0(AutoConnect, void());
  MOCK_METHOD1(SetLastGoodApn, void(const Stringmap &apn_info));
  MOCK_METHOD0(ClearLastGoodApn, void());
  MOCK_METHOD1(SetActivationState, void(const std::string &state));
  MOCK_METHOD1(Disconnect, void(Error *error));
  MOCK_METHOD1(SetState, void(ConnectState state));
  MOCK_METHOD1(SetFailure, void(ConnectFailure failure));
  MOCK_METHOD1(SetFailureSilent, void(ConnectFailure failure));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCellularService);
};

}  // namespace shill

#endif  // SHILL_MOCK_CELLULAR_SERVICE_H_
