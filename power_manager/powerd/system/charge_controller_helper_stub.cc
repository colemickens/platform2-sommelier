// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/charge_controller_helper_stub.h"

namespace power_manager {
namespace system {

constexpr int ChargeControllerHelperStub::kThresholdUnset = -1;

ChargeControllerHelperStub::ChargeControllerHelperStub() = default;

ChargeControllerHelperStub::~ChargeControllerHelperStub() = default;

bool ChargeControllerHelperStub::enabled() const {
  return enabled_;
}

int ChargeControllerHelperStub::threshold() const {
  return threshold_;
}

std::string ChargeControllerHelperStub::day_config(WeekDay day) const {
  const auto& it = day_configs_.find(day);
  return it != day_configs_.end() ? it->second : std::string();
}

bool ChargeControllerHelperStub::SetPeakShiftEnabled(bool enable) {
  enabled_ = enable;
  return true;
}

bool ChargeControllerHelperStub::SetPeakShiftBatteryPercentThreshold(
    int threshold) {
  threshold_ = threshold;
  return true;
}

bool ChargeControllerHelperStub::SetPeakShiftDayConfig(
    WeekDay week_day, const std::string& config) {
  day_configs_[week_day] = config;
  return true;
}

void ChargeControllerHelperStub::Reset() {
  enabled_ = false;
  threshold_ = -1;
  day_configs_.clear();
}

}  // namespace system
}  // namespace power_manager
