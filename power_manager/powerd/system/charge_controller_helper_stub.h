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

  bool peak_shift_enabled() const;
  int peak_shift_threshold() const;
  std::string peak_shift_day_config(WeekDay day) const;

  bool boot_on_ac_enabled() const;

  bool usb_power_share_enabled() const;

  // ChargeControllerHelperInterface overrides:
  bool SetPeakShiftEnabled(bool enable) override;
  bool SetPeakShiftBatteryPercentThreshold(int threshold) override;
  bool SetPeakShiftDayConfig(WeekDay week_day,
                             const std::string& config) override;
  bool SetBootOnAcEnabled(bool enable) override;
  bool SetUsbPowerShareEnabled(bool enable) override;

  void Reset();

 private:
  bool peak_shift_enabled_ = false;
  int peak_shift_threshold_ = kThresholdUnset;
  std::map<WeekDay, std::string> peak_shift_day_configs_;

  bool boot_on_ac_enabled_ = false;

  bool usb_power_share_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(ChargeControllerHelperStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_STUB_H_
