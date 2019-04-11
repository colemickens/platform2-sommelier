// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/charge_controller.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "power_manager/powerd/system/charge_controller_helper_interface.h"

namespace power_manager {
namespace policy {

namespace {

std::string GetPeakShiftDayConfigTimeDebugString(
    const PowerManagementPolicy::PeakShiftDayConfig& day_config) {
  return base::StringPrintf(
      "%02d:%02d %02d:%02d %02d:%02d", day_config.start_time().hour(),
      day_config.start_time().minute(), day_config.end_time().hour(),
      day_config.end_time().minute(), day_config.charge_start_time().hour(),
      day_config.charge_start_time().minute());
}

std::string GetPeakShiftDayConfigDebugString(
    const PowerManagementPolicy::PeakShiftDayConfig& day_config) {
  return base::StringPrintf(
      "{day=%d time=%s}", static_cast<int>(day_config.day()),
      GetPeakShiftDayConfigTimeDebugString(day_config).c_str());
}

std::string GetPowerPolicyDebugString(const PowerManagementPolicy& policy) {
  std::string str;
  if (policy.has_peak_shift_battery_percent_threshold()) {
    str += "peak_shift_battery_percent_threshold=" +
           base::IntToString(policy.peak_shift_battery_percent_threshold()) +
           " ";
  }
  if (policy.peak_shift_day_configs_size()) {
    str += "peak_shift_day_configs=[";
    str += GetPeakShiftDayConfigDebugString(policy.peak_shift_day_configs(0));
    for (int i = 1; i < policy.peak_shift_day_configs_size(); i++) {
      str += ", " +
             GetPeakShiftDayConfigDebugString(policy.peak_shift_day_configs(i));
    }
    str += "] ";
  }

  if (policy.has_boot_on_ac()) {
    str += "boot_on_ac=";
    str += policy.boot_on_ac() ? "true " : "false ";
  }

  base::TrimString(str, " ", &str);
  return str;
}

}  // namespace

ChargeController::ChargeController() = default;

ChargeController::~ChargeController() = default;

void ChargeController::Init(system::ChargeControllerHelperInterface* helper) {
  DCHECK(helper);
  helper_ = helper;
}

void ChargeController::HandlePolicyChange(const PowerManagementPolicy& policy) {
  if (IsPolicyEqualToCache(policy)) {
    return;
  }

  LOG(INFO) << "Received updated power policies: "
            << GetPowerPolicyDebugString(policy);

  if (ApplyPolicyChange(policy)) {
    cached_policy_ = policy;
  } else {
    cached_policy_.reset();
  }
}

bool ChargeController::ApplyPolicyChange(const PowerManagementPolicy& policy) {
  DCHECK(helper_);

  // Try to apply as many changes as possible.
  return ApplyPeakShiftChange(policy) & ApplyBootOnAcChange(policy);
}

bool ChargeController::ApplyPeakShiftChange(
    const PowerManagementPolicy& policy) {
  if (!policy.has_peak_shift_battery_percent_threshold() ||
      policy.peak_shift_day_configs_size() == 0) {
    return helper_->SetPeakShiftEnabled(false /* enable */);
  }

  if (!helper_->SetPeakShiftEnabled(true /* enable */)) {
    return false;
  }
  if (!helper_->SetPeakShiftBatteryPercentThreshold(
          policy.peak_shift_battery_percent_threshold())) {
    return false;
  }
  for (int i = 0; i < policy.peak_shift_day_configs_size(); i++) {
    if (!SetPeakShiftDayConfig(policy.peak_shift_day_configs(i))) {
      return false;
    }
  }

  return true;
}

bool ChargeController::ApplyBootOnAcChange(
    const PowerManagementPolicy& policy) {
  // Disable if |boot_on_ac| is unset.
  return helper_->SetBootOnAcEnabled(policy.boot_on_ac() /* enable */);
}

bool ChargeController::SetPeakShiftDayConfig(
    const PowerManagementPolicy::PeakShiftDayConfig& day_config) {
  if (!day_config.has_day() || !day_config.has_start_time() ||
      !day_config.start_time().has_hour() ||
      !day_config.start_time().has_minute() || !day_config.has_end_time() ||
      !day_config.end_time().has_hour() ||
      !day_config.end_time().has_minute() ||
      !day_config.has_charge_start_time() ||
      !day_config.charge_start_time().has_hour() ||
      !day_config.charge_start_time().has_minute()) {
    LOG(WARNING) << "Invalid peak shift day config proto";
    return false;
  }

  system::ChargeControllerHelperInterface::WeekDay week_day;
  switch (day_config.day()) {
    case PowerManagementPolicy::MONDAY:
      week_day = system::ChargeControllerHelperInterface::WeekDay::MONDAY;
      break;
    case PowerManagementPolicy::TUESDAY:
      week_day = system::ChargeControllerHelperInterface::WeekDay::TUESDAY;
      break;
    case PowerManagementPolicy::WEDNESDAY:
      week_day = system::ChargeControllerHelperInterface::WeekDay::WEDNESDAY;
      break;
    case PowerManagementPolicy::THURSDAY:
      week_day = system::ChargeControllerHelperInterface::WeekDay::THURSDAY;
      break;
    case PowerManagementPolicy::FRIDAY:
      week_day = system::ChargeControllerHelperInterface::WeekDay::FRIDAY;
      break;
    case PowerManagementPolicy::SATURDAY:
      week_day = system::ChargeControllerHelperInterface::WeekDay::SATURDAY;
      break;
    case PowerManagementPolicy::SUNDAY:
      week_day = system::ChargeControllerHelperInterface::WeekDay::SUNDAY;
      break;
    default:
      LOG(WARNING) << "Invalid peak shift day value: " << day_config.day();
      return false;
  }

  std::string day_config_str = base::StringPrintf(
      "%02d %02d %02d %02d %02d %02d", day_config.start_time().hour(),
      day_config.start_time().minute(), day_config.end_time().hour(),
      day_config.end_time().minute(), day_config.charge_start_time().hour(),
      day_config.charge_start_time().minute());
  return helper_->SetPeakShiftDayConfig(week_day, day_config_str);
}

bool ChargeController::IsPolicyEqualToCache(
    const PowerManagementPolicy& policy) const {
  if (!cached_policy_.has_value()) {
    return false;
  }

  if (policy.peak_shift_battery_percent_threshold() !=
      cached_policy_->peak_shift_battery_percent_threshold()) {
    return false;
  }

  if (policy.peak_shift_day_configs_size() !=
      cached_policy_->peak_shift_day_configs_size()) {
    return false;
  }
  for (int i = 0; i < policy.peak_shift_day_configs_size(); i++) {
    std::string policy_day_config, cached_policy_day_config;
    if (!policy.peak_shift_day_configs(i).SerializeToString(
            &policy_day_config) ||
        !cached_policy_->peak_shift_day_configs(i).SerializeToString(
            &cached_policy_day_config) ||
        policy_day_config != cached_policy_day_config) {
      return false;
    }
  }

  if (policy.boot_on_ac() != cached_policy_->boot_on_ac()) {
    return false;
  }
  return true;
}

}  // namespace policy
}  // namespace power_manager
