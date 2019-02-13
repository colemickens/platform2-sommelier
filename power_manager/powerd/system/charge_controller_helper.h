// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_H_
#define POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_H_

#include <string>

#include <base/macros.h>

#include "power_manager/powerd/system/charge_controller_helper_interface.h"

namespace power_manager {
namespace system {

// Real implementation of ChargeControllerHelperInterface.
class ChargeControllerHelper final : public ChargeControllerHelperInterface {
 public:
  ChargeControllerHelper();
  ~ChargeControllerHelper() override;

  // ChargeControllerHelperInterface overrides:
  bool SetPeakShiftEnabled(bool enable) override;
  bool SetPeakShiftBatteryPercentThreshold(int threshold) override;
  bool SetPeakShiftDayConfig(WeekDay week_day,
                             const std::string& config) override;

  DISALLOW_COPY_AND_ASSIGN(ChargeControllerHelper);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_H_