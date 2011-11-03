// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_POWER_MANAGER_PROXY_H_
#define SHILL_MOCK_POWER_MANAGER_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/power_manager_proxy_interface.h"

namespace shill {

class MockPowerManagerProxy : public PowerManagerProxyInterface {
 public:
  MockPowerManagerProxy();
  virtual ~MockPowerManagerProxy();

  MOCK_METHOD1(RegisterSuspendDelay, void(uint32 delay_ms));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPowerManagerProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_POWER_MANAGER_PROXY_H_
