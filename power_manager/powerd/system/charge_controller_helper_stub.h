// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_STUB_H_

#include <map>
#include <string>

#include <base/macros.h>

#include "power_manager/powerd/system/charge_controller_helper_interface.h"

namespace power_manager {
namespace system {

// Stub implementation of ChargeControllerHelperInterface for use by tests.
class ChargeControllerHelperStub : public ChargeControllerHelperInterface {
 public:
  static const int kThresholdUnset;

  ChargeControllerHelperStub();
  ~ChargeControllerHelperStub() override;

  bool enabled() const;
  int threshold() const;
  std::string day_config(WeekDay day) const;

  // ChargeControllerHelperInterface overrides:
  bool SetPeakShiftEnabled(bool enable) override;
  bool SetPeakShiftBatteryPercentThreshold(int threshold) override;
  bool SetPeakShiftDayConfig(WeekDay week_day,
                             const std::string& config) override;

  void Reset();

 private:
  bool enabled_ = false;
  int threshold_ = kThresholdUnset;
  std::map<WeekDay, std::string> day_configs_;

  DISALLOW_COPY_AND_ASSIGN(ChargeControllerHelperStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_STUB_H_
