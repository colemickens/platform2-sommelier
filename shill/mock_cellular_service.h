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
  MockCellularService(ControlInterface *control_interface,
                      EventDispatcher *dispatcher,
                      Metrics *metrics,
                      Manager *manager,
                      const CellularRefPtr &device);
  virtual ~MockCellularService();

  MOCK_METHOD1(SetLastGoodApn, void(const Stringmap &apn_info));
  MOCK_METHOD0(ClearLastGoodApn, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCellularService);
};

}  // namespace shill

#endif  // SHILL_MOCK_CELLULAR_SERVICE_H_
