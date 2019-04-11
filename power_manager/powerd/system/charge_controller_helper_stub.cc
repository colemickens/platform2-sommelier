// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/charge_controller_helper_stub.h"

namespace power_manager {
namespace system {

constexpr int ChargeControllerHelperStub::kThresholdUnset = -1;

bool ChargeControllerHelperStub::SetPeakShiftEnabled(bool enable) {
  peak_shift_enabled_ = enable;
  return true;
}

bool ChargeControllerHelperStub::SetPeakShiftBatteryPercentThreshold(
    int threshold) {
  peak_shift_threshold_ = threshold;
  return true;
}

bool ChargeControllerHelperStub::SetPeakShiftDayConfig(
    PowerManagementPolicy::WeekDay week_day, const std::string& config) {
  peak_shift_day_configs_[week_day] = config;
  return true;
}

bool ChargeControllerHelperStub::SetBootOnAcEnabled(bool enable) {
  boot_on_ac_enabled_ = enable;
  return true;
}

bool ChargeControllerHelperStub::SetUsbPowerShareEnabled(bool enable) {
  usb_power_share_enabled_ = enable;
  return true;
}

bool ChargeControllerHelperStub::SetAdvancedBatteryChargeModeEnabled(
    bool enable) {
  advanced_battery_charge_mode_enabled_ = enable;
  return true;
}

bool ChargeControllerHelperStub::SetAdvancedBatteryChargeModeDayConfig(
    PowerManagementPolicy::WeekDay week_day, const std::string& config) {
  advanced_battery_charge_mode_day_configs_[week_day] = config;
  return true;
}

void ChargeControllerHelperStub::Reset() {
  peak_shift_enabled_ = false;
  peak_shift_threshold_ = kThresholdUnset;
  peak_shift_day_configs_.clear();

  boot_on_ac_enabled_ = false;

  usb_power_share_enabled_ = false;

  advanced_battery_charge_mode_enabled_ = false;
  advanced_battery_charge_mode_day_configs_.clear();
}

}  // namespace system
}  // namespace power_manager
