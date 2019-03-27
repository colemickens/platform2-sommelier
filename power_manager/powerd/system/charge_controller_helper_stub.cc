// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/charge_controller_helper_stub.h"

namespace power_manager {
namespace system {

constexpr int ChargeControllerHelperStub::kThresholdUnset = -1;

ChargeControllerHelperStub::ChargeControllerHelperStub() = default;

ChargeControllerHelperStub::~ChargeControllerHelperStub() = default;

bool ChargeControllerHelperStub::peak_shift_enabled() const {
  return peak_shift_enabled_;
}

int ChargeControllerHelperStub::peak_shift_threshold() const {
  return peak_shift_threshold_;
}

std::string ChargeControllerHelperStub::peak_shift_day_config(
    WeekDay day) const {
  const auto& it = peak_shift_day_configs_.find(day);
  return it != peak_shift_day_configs_.end() ? it->second : std::string();
}

bool ChargeControllerHelperStub::boot_on_ac_enabled() const {
  return boot_on_ac_enabled_;
}

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
    WeekDay week_day, const std::string& config) {
  peak_shift_day_configs_[week_day] = config;
  return true;
}

bool ChargeControllerHelperStub::SetBootOnAcEnabled(bool enable) {
  boot_on_ac_enabled_ = enable;
  return true;
}

void ChargeControllerHelperStub::Reset() {
  peak_shift_enabled_ = false;
  peak_shift_threshold_ = -1;
  peak_shift_day_configs_.clear();

  boot_on_ac_enabled_ = false;
}

}  // namespace system
}  // namespace power_manager
