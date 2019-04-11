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

  ChargeControllerHelperStub() = default;
  ~ChargeControllerHelperStub() override = default;

  bool peak_shift_enabled() const { return peak_shift_enabled_; }
  int peak_shift_threshold() const { return peak_shift_threshold_; }
  std::string peak_shift_day_config(PowerManagementPolicy::WeekDay day) const {
    const auto& it = peak_shift_day_configs_.find(day);
    return it != peak_shift_day_configs_.end() ? it->second : std::string();
  }

  bool boot_on_ac_enabled() const { return boot_on_ac_enabled_; }

  bool usb_power_share_enabled() const { return usb_power_share_enabled_; }

  bool advanced_battery_charge_mode_enabled() const {
    return advanced_battery_charge_mode_enabled_;
  }
  std::string advanced_battery_charge_mode_day_config(
      PowerManagementPolicy::WeekDay day) const {
    const auto& it = advanced_battery_charge_mode_day_configs_.find(day);
    return it != advanced_battery_charge_mode_day_configs_.end()
               ? it->second
               : std::string();
  }

  // ChargeControllerHelperInterface overrides:
  bool SetPeakShiftEnabled(bool enable) override;
  bool SetPeakShiftBatteryPercentThreshold(int threshold) override;
  bool SetPeakShiftDayConfig(PowerManagementPolicy::WeekDay week_day,
                             const std::string& config) override;
  bool SetBootOnAcEnabled(bool enable) override;
  bool SetUsbPowerShareEnabled(bool enable) override;
  bool SetAdvancedBatteryChargeModeEnabled(bool enable) override;
  bool SetAdvancedBatteryChargeModeDayConfig(
      PowerManagementPolicy::WeekDay week_day,
      const std::string& config) override;

  void Reset();

 private:
  bool peak_shift_enabled_ = false;
  int peak_shift_threshold_ = kThresholdUnset;
  std::map<PowerManagementPolicy::WeekDay, std::string> peak_shift_day_configs_;

  bool boot_on_ac_enabled_ = false;

  bool usb_power_share_enabled_ = false;

  bool advanced_battery_charge_mode_enabled_ = false;
  std::map<PowerManagementPolicy::WeekDay, std::string>
      advanced_battery_charge_mode_day_configs_;

  DISALLOW_COPY_AND_ASSIGN(ChargeControllerHelperStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_STUB_H_
