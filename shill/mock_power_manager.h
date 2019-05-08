// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_POWER_MANAGER_H_
#define SHILL_MOCK_POWER_MANAGER_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/power_manager.h"

namespace shill {

class ControlInterface;

class MockPowerManager : public PowerManager {
 public:
  explicit MockPowerManager(ControlInterface* control_interface);
  ~MockPowerManager() override;

  MOCK_METHOD0(ReportSuspendReadiness, bool());
  MOCK_METHOD0(ReportDarkSuspendReadiness, bool());
  MOCK_METHOD4(
      Start,
      void(base::TimeDelta suspend_delay,
           const PowerManager::SuspendImminentCallback& imminent_callback,
           const PowerManager::SuspendDoneCallback& done_callback,
           const PowerManager::DarkSuspendImminentCallback& dark_imminent));
  MOCK_METHOD0(Stop, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPowerManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_POWER_MANAGER_H_
