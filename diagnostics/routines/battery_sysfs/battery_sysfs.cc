// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/battery_sysfs/battery_sysfs.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

#include "diagnostics/common/mojo_utils.h"

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

namespace {

uint32_t CalculateProgressPercent(
    mojo_ipc::DiagnosticRoutineStatusEnum status) {
  // Since the battery_sysfs routine cannot be cancelled, the progress percent
  // can only be 0 or 100.
  if (status == mojo_ipc::DiagnosticRoutineStatusEnum::kPassed ||
      status == mojo_ipc::DiagnosticRoutineStatusEnum::kFailed)
    return 100;
  return 0;
}

const struct {
  const char* battery_log_key;
  const char* relative_file_path;
} kBatteryLogKeyPaths[] = {
    {"Manufacturer", kBatterySysfsManufacturerPath},
    {"Current Now", kBatterySysfsCurrentNowPath},
    {"Present", kBatterySysfsPresentPath},
    {"Status", kBatterySysfsStatusPath},
    {"Voltage Now", kBatterySysfsVoltageNowPath},
    {"Charge Full", kBatterySysfsChargeFullPath},
    {"Charge Full Design", kBatterySysfsChargeFullDesignPath},
    {"Charge Now", kBatterySysfsChargeNowPath}};

bool TryReadFileToString(const base::FilePath& absolute_file_path,
                         std::string* file_contents) {
  DCHECK(file_contents);

  if (!base::ReadFileToString(absolute_file_path, file_contents))
    return false;

  base::TrimWhitespaceASCII(*file_contents, base::TRIM_TRAILING, file_contents);

  return true;
}

bool TryReadFileToUint(const base::FilePath& absolute_file_path,
                       uint32_t* output) {
  DCHECK(output);

  std::string file_contents;
  if (!base::ReadFileToString(absolute_file_path, &file_contents))
    return false;

  base::TrimWhitespaceASCII(file_contents, base::TRIM_TRAILING, &file_contents);
  if (!base::StringToUint(file_contents, output))
    return false;

  return true;
}

}  // namespace

const char kBatterySysfsPath[] = "sys/class/power_supply/BAT0/";
const char kBatterySysfsChargeFullPath[] = "charge_full";
const char kBatterySysfsChargeFullDesignPath[] = "charge_full_design";
const char kBatterySysfsEnergyFullPath[] = "energy_full";
const char kBatterySysfsEnergyFullDesignPath[] = "energy_full_design";
const char kBatterySysfsCycleCountPath[] = "cycle_count";
const char kBatterySysfsManufacturerPath[] = "manufacturer";
const char kBatterySysfsCurrentNowPath[] = "current_now";
const char kBatterySysfsPresentPath[] = "present";
const char kBatterySysfsStatusPath[] = "status";
const char kBatterySysfsVoltageNowPath[] = "voltage_now";
const char kBatterySysfsChargeNowPath[] = "charge_now";

const char kBatterySysfsInvalidParametersMessage[] =
    "Invalid battery sysfs routine parameters.";
const char kBatterySysfsFailedCalculatingWearPercentageMessage[] =
    "Could not get wear percentage.";
const char kBatterySysfsExcessiveWearMessage[] = "Battery is over-worn.";
const char kBatterySysfsFailedReadingCycleCountMessage[] =
    "Could not get cycle count.";
const char kBatterySysfsExcessiveCycleCountMessage[] =
    "Battery cycle count is too high.";
const char kBatterySysfsRoutinePassedMessage[] = "Routine passed.";

BatterySysfsRoutine::BatterySysfsRoutine(uint32_t maximum_cycle_count,
                                         uint32_t percent_battery_wear_allowed)
    : status_(mojo_ipc::DiagnosticRoutineStatusEnum::kReady),
      maximum_cycle_count_(maximum_cycle_count),
      percent_battery_wear_allowed_(percent_battery_wear_allowed) {}

BatterySysfsRoutine::~BatterySysfsRoutine() = default;

void BatterySysfsRoutine::Start() {
  DCHECK_EQ(status_, mojo_ipc::DiagnosticRoutineStatusEnum::kReady);
  if (!RunBatterySysfsRoutine())
    LOG(ERROR) << "Routine failed: " << status_message_;
}

// The battery_sysfs routine can only be started.
void BatterySysfsRoutine::Resume() {}
void BatterySysfsRoutine::Cancel() {}

void BatterySysfsRoutine::PopulateStatusUpdate(
    mojo_ipc::RoutineUpdate* response, bool include_output) {
  // Because the battery_sysfs routine is non-interactive, we will never include
  // a user message.
  mojo_ipc::NonInteractiveRoutineUpdate update;
  update.status = status_;
  update.status_message = status_message_;

  response->routine_update_union->set_noninteractive_update(update.Clone());
  response->progress_percent = CalculateProgressPercent(status_);

  if (include_output) {
    std::string output;
    for (const auto& key_val : battery_sysfs_log_)
      output += key_val.first + ": " + key_val.second + "\n";
    response->output =
        CreateReadOnlySharedMemoryMojoHandle(base::StringPiece(output));
  }
}

mojo_ipc::DiagnosticRoutineStatusEnum BatterySysfsRoutine::GetStatus() {
  return status_;
}

void BatterySysfsRoutine::set_root_dir_for_testing(
    const base::FilePath& root_dir) {
  root_dir_ = root_dir;
}

bool BatterySysfsRoutine::RunBatterySysfsRoutine() {
  for (const auto& item : kBatteryLogKeyPaths) {
    std::string file_contents;
    base::FilePath absolute_file_path(
        root_dir_.AppendASCII(kBatterySysfsPath)
            .AppendASCII(item.relative_file_path));
    if (!TryReadFileToString(absolute_file_path, &file_contents)) {
      // Failing to read and log a file should not cause the routine to fail,
      // but we should record the event.
      PLOG(WARNING) << "Battery attribute unavailable: "
                    << item.battery_log_key;
      continue;
    }

    battery_sysfs_log_[item.battery_log_key] = file_contents;
  }

  if (!TestWearPercentage() || !TestCycleCount())
    return false;

  status_message_ = kBatterySysfsRoutinePassedMessage;
  status_ = mojo_ipc::DiagnosticRoutineStatusEnum::kPassed;
  return true;
}

bool BatterySysfsRoutine::ReadBatteryCapacities(uint32_t* capacity,
                                                uint32_t* design_capacity) {
  DCHECK(capacity);
  DCHECK(design_capacity);

  base::FilePath absolute_charge_full_path(
      root_dir_.AppendASCII(kBatterySysfsPath)
          .AppendASCII(kBatterySysfsChargeFullPath));
  base::FilePath absolute_charge_full_design_path(
      root_dir_.AppendASCII(kBatterySysfsPath)
          .AppendASCII(kBatterySysfsChargeFullDesignPath));
  if (!TryReadFileToUint(absolute_charge_full_path, capacity) ||
      !TryReadFileToUint(absolute_charge_full_design_path, design_capacity)) {
    // No charge values, check for energy-reporting batteries.
    base::FilePath absolute_energy_full_path(
        root_dir_.AppendASCII(kBatterySysfsPath)
            .AppendASCII(kBatterySysfsEnergyFullPath));
    base::FilePath absolute_energy_full_design_path(
        root_dir_.AppendASCII(kBatterySysfsPath)
            .AppendASCII(kBatterySysfsEnergyFullDesignPath));
    if (!TryReadFileToUint(absolute_energy_full_path, capacity) ||
        !TryReadFileToUint(absolute_energy_full_design_path, design_capacity)) {
      return false;
    }
  }

  return true;
}

bool BatterySysfsRoutine::ReadCycleCount(uint32_t* cycle_count) {
  DCHECK(cycle_count);

  base::FilePath absolute_cycle_count_path(
      root_dir_.AppendASCII(kBatterySysfsPath)
          .AppendASCII(kBatterySysfsCycleCountPath));
  if (!TryReadFileToUint(absolute_cycle_count_path, cycle_count))
    return false;

  return true;
}

bool BatterySysfsRoutine::TestWearPercentage() {
  uint32_t capacity;
  uint32_t design_capacity;

  if (percent_battery_wear_allowed_ > 100) {
    status_message_ = kBatterySysfsInvalidParametersMessage;
    status_ = mojo_ipc::DiagnosticRoutineStatusEnum::kError;
    return false;
  }

  if (!ReadBatteryCapacities(&capacity, &design_capacity) || capacity < 0 ||
      design_capacity < 0) {
    status_message_ = kBatterySysfsFailedCalculatingWearPercentageMessage;
    status_ = mojo_ipc::DiagnosticRoutineStatusEnum::kError;
    return false;
  }

  // Cap the wear percentage at 0. There are cases where the capacity can be
  // higher than the design capacity, due to variance in batteries or vendors
  // setting conservative design capacities.
  uint32_t wear_percentage =
      capacity > design_capacity ? 0 : 100 - capacity * 100 / design_capacity;

  battery_sysfs_log_["Wear Percentage"] = std::to_string(wear_percentage);
  if (wear_percentage > percent_battery_wear_allowed_) {
    status_message_ = kBatterySysfsExcessiveWearMessage;
    status_ = mojo_ipc::DiagnosticRoutineStatusEnum::kFailed;
    return false;
  }

  return true;
}

bool BatterySysfsRoutine::TestCycleCount() {
  uint32_t cycle_count;
  if (!ReadCycleCount(&cycle_count) || cycle_count < 0) {
    status_message_ = kBatterySysfsFailedReadingCycleCountMessage;
    status_ = mojo_ipc::DiagnosticRoutineStatusEnum::kError;
    return false;
  }

  battery_sysfs_log_["Cycle Count"] = std::to_string(cycle_count);
  if (cycle_count > maximum_cycle_count_) {
    status_message_ = kBatterySysfsExcessiveCycleCountMessage;
    status_ = mojo_ipc::DiagnosticRoutineStatusEnum::kFailed;
    return false;
  }

  return true;
}

}  // namespace diagnostics
