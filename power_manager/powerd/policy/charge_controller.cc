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

std::string GetWeekDayDebugString(
    PowerManagementPolicy::WeekDay proto_week_day) {
  switch (proto_week_day) {
    case PowerManagementPolicy::MONDAY:
      return "monday";
    case PowerManagementPolicy::TUESDAY:
      return "tuesday";
    case PowerManagementPolicy::WEDNESDAY:
      return "wednesday";
    case PowerManagementPolicy::THURSDAY:
      return "thursday";
    case PowerManagementPolicy::FRIDAY:
      return "friday";
    case PowerManagementPolicy::SATURDAY:
      return "saturday";
    case PowerManagementPolicy::SUNDAY:
      return "sunday";
  }
  return base::StringPrintf("invalid (%d)", proto_week_day);
}

std::string GetPeakShiftDayConfigDebugString(
    const PowerManagementPolicy::PeakShiftDayConfig& day_config) {
  return base::StringPrintf(
      "{day=%s time=%02d:%02d %02d:%02d %02d:%02d}",
      GetWeekDayDebugString(day_config.day()).c_str(),
      day_config.start_time().hour(), day_config.start_time().minute(),
      day_config.end_time().hour(), day_config.end_time().minute(),
      day_config.charge_start_time().hour(),
      day_config.charge_start_time().minute());
}

std::string GetAdvancedBatteryChargeModeDayConfigDebugString(
    const PowerManagementPolicy::AdvancedBatteryChargeModeDayConfig&
        day_config) {
  return base::StringPrintf("{day=%s time=%02d:%02d %02d:%02d}",
                            GetWeekDayDebugString(day_config.day()).c_str(),
                            day_config.charge_start_time().hour(),
                            day_config.charge_start_time().minute(),
                            day_config.charge_end_time().hour(),
                            day_config.charge_end_time().minute());
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

  if (policy.has_usb_power_share()) {
    str += "usb_power_share=";
    str += policy.usb_power_share() ? "true " : "false ";
  }

  if (policy.advanced_battery_charge_mode_day_configs_size()) {
    str += "advanced_battery_charge_mode_day_configs=[";
    str += GetAdvancedBatteryChargeModeDayConfigDebugString(
        policy.advanced_battery_charge_mode_day_configs(0));
    for (int i = 1; i < policy.advanced_battery_charge_mode_day_configs_size();
         i++) {
      str += ", " + GetAdvancedBatteryChargeModeDayConfigDebugString(
                        policy.advanced_battery_charge_mode_day_configs(i));
    }
    str += "] ";
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
  return ApplyPeakShiftChange(policy) & ApplyBootOnAcChange(policy) &
         ApplyUsbPowerShareChange(policy) &
         ApplyAdvancedBatteryChargeModeChange(policy);
}

bool ChargeController::ApplyPeakShiftChange(
    const PowerManagementPolicy& policy) {
  if (!policy.has_peak_shift_battery_percent_threshold() ||
      policy.peak_shift_day_configs_size() == 0) {
    return helper_->SetPeakShiftEnabled(false);
  }

  if (!helper_->SetPeakShiftEnabled(true)) {
    return false;
  }
  if (!helper_->SetPeakShiftBatteryPercentThreshold(
          policy.peak_shift_battery_percent_threshold())) {
    return false;
  }
  for (const auto& day_config : policy.peak_shift_day_configs()) {
    if (!SetPeakShiftDayConfig(day_config)) {
      return false;
    }
  }

  return true;
}

bool ChargeController::ApplyBootOnAcChange(
    const PowerManagementPolicy& policy) {
  // Disable if |boot_on_ac| is unset.
  return helper_->SetBootOnAcEnabled(policy.boot_on_ac());
}

bool ChargeController::ApplyUsbPowerShareChange(
    const PowerManagementPolicy& policy) {
  // Disable if |usb_power_share| is unset.
  return helper_->SetUsbPowerShareEnabled(policy.usb_power_share());
}

bool ChargeController::ApplyAdvancedBatteryChargeModeChange(
    const PowerManagementPolicy& policy) {
  if (policy.advanced_battery_charge_mode_day_configs_size() == 0) {
    return helper_->SetAdvancedBatteryChargeModeEnabled(false);
  }

  if (!helper_->SetAdvancedBatteryChargeModeEnabled(true)) {
    return false;
  }
  for (const auto& day_config :
       policy.advanced_battery_charge_mode_day_configs()) {
    if (!SetAdvancedBatteryChargeModeDayConfig(day_config)) {
      return false;
    }
  }
  return true;
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

  std::string day_config_str = base::StringPrintf(
      "%02d %02d %02d %02d %02d %02d", day_config.start_time().hour(),
      day_config.start_time().minute(), day_config.end_time().hour(),
      day_config.end_time().minute(), day_config.charge_start_time().hour(),
      day_config.charge_start_time().minute());
  return helper_->SetPeakShiftDayConfig(day_config.day(), day_config_str);
}

bool ChargeController::SetAdvancedBatteryChargeModeDayConfig(
    const PowerManagementPolicy::AdvancedBatteryChargeModeDayConfig&
        day_config) {
  if (!day_config.has_day() || !day_config.has_charge_start_time() ||
      !day_config.charge_start_time().has_hour() ||
      !day_config.charge_start_time().has_minute() ||
      !day_config.has_charge_end_time() ||
      !day_config.charge_end_time().has_hour() ||
      !day_config.charge_end_time().has_minute()) {
    LOG(WARNING) << "Invalid advanced battery charge mode day config proto";
    return false;
  }

  int start_time_minutes = day_config.charge_start_time().hour() * 60 +
                           day_config.charge_start_time().minute();
  int end_time_minutes = day_config.charge_end_time().hour() * 60 +
                         day_config.charge_end_time().minute();
  if (start_time_minutes > end_time_minutes) {
    LOG(WARNING) << "Invalid advanced battery charge mode day config proto:"
                 << " start time must be less or equal than end time";
    return false;
  }

  // Policy uses charge end time, but EC driver uses charge duration.
  int duration_minutes = end_time_minutes - start_time_minutes;
  std::string day_config_str = base::StringPrintf(
      "%02d %02d %02d %02d", day_config.charge_start_time().hour(),
      day_config.charge_start_time().minute(), duration_minutes / 60,
      duration_minutes % 60);
  return helper_->SetAdvancedBatteryChargeModeDayConfig(day_config.day(),
                                                        day_config_str);
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

  if (policy.usb_power_share() != cached_policy_->usb_power_share()) {
    return false;
  }

  if (policy.advanced_battery_charge_mode_day_configs_size() !=
      cached_policy_->advanced_battery_charge_mode_day_configs_size()) {
    return false;
  }
  for (int i = 0; i < policy.advanced_battery_charge_mode_day_configs_size();
       i++) {
    std::string policy_day_config, cached_policy_day_config;
    if (!policy.advanced_battery_charge_mode_day_configs(i).SerializeToString(
            &policy_day_config) ||
        !cached_policy_->advanced_battery_charge_mode_day_configs(i)
             .SerializeToString(&cached_policy_day_config) ||
        policy_day_config != cached_policy_day_config) {
      return false;
    }
  }

  return true;
}

}  // namespace policy
}  // namespace power_manager
