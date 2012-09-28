// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_POWER_MANAGER_H_
#define SHILL_MOCK_POWER_MANAGER_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/power_manager.h"

namespace shill {

class ProxyFactory;

class MockPowerManager : public PowerManager {
 public:
  MockPowerManager(EventDispatcher *dispatcher, ProxyFactory *proxy_factory);
  virtual ~MockPowerManager();

  MOCK_METHOD2(AddStateChangeCallback,
               void(const std::string &key,
                    const PowerStateCallback &callback));
  MOCK_METHOD2(AddSuspendDelayCallback,
               void(const std::string &key,
                    const SuspendDelayCallback &callback));
  MOCK_METHOD1(RemoveStateChangeCallback, void(const std::string &key));
  MOCK_METHOD1(RemoveSuspendDelayCallback, void(const std::string &key));
  MOCK_METHOD1(RegisterSuspendDelay, void(uint32 delay_ms));
  MOCK_METHOD0(UnregisterSuspendDelay, void());
  MOCK_METHOD1(SuspendReady, void(uint32 suspend_ready));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPowerManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_POWER_MANAGER_H_
