// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_POWER_MANAGER_H_
#define SHILL_MOCK_POWER_MANAGER_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/power_manager.h"

namespace shill {

class ProxyFactory;

class MockPowerManager : public PowerManager {
 public:
  MockPowerManager(EventDispatcher *dispatcher, ProxyFactory *proxy_factory);
  virtual ~MockPowerManager();

  MOCK_METHOD5(AddSuspendDelay,
               bool(const std::string &key,
                    const std::string &description,
                    base::TimeDelta timeout,
                    const SuspendImminentCallback &immiment_callback,
                    const SuspendDoneCallback &done_callback));
  MOCK_METHOD1(RemoveSuspendDelay, bool(const std::string &key));
  MOCK_METHOD2(ReportSuspendReadiness,
               bool(const std::string &key, int suspend_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPowerManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_POWER_MANAGER_H_
