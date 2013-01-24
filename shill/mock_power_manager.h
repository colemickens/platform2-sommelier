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
  MOCK_METHOD3(RegisterSuspendDelay,
               bool(base::TimeDelta timeout,
                    const std::string &description,
                    int *delay_id_out));
  MOCK_METHOD1(UnregisterSuspendDelay, bool(int delay_id));
  MOCK_METHOD2(ReportSuspendReadiness, bool(int delay_id, int suspend_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPowerManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_POWER_MANAGER_H_
