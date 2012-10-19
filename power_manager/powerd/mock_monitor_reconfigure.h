// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_MOCK_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_POWERD_MOCK_MONITOR_RECONFIGURE_H_

#include <gmock/gmock.h>

#include "power_manager/powerd/monitor_reconfigure.h"

namespace power_manager {

class MockMonitorReconfigure : public MonitorReconfigureInterface {
 public:
  MOCK_METHOD1(set_is_internal_panel_enabled, void(bool));
  MOCK_METHOD2(SetScreenPowerState,
               void(ScreenPowerOutputSelection, ScreenPowerState));

  // Adds an expectation that SetScreenPowerState() will be called once.
  void ExpectRequest(ScreenPowerOutputSelection selection,
                     ScreenPowerState state) {
    EXPECT_CALL(*this, SetScreenPowerState(selection, state));
  }
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_MOCK_MONITOR_RECONFIGURE_H_
