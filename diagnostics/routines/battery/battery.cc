// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/battery/battery.h"

#include <cstdint>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

namespace {
// Conversion factor from uAh to mAh.
constexpr uint32_t kuAhTomAhDivisor = 1000;

uint32_t CalculateProgressPercent(
    mojo_ipc::DiagnosticRoutineStatusEnum status) {
  // Since the battery test cannot be cancelled, the progress percent can only
  // be 0 or 100.
  if (status == mojo_ipc::DiagnosticRoutineStatusEnum::kPassed ||
      status == mojo_ipc::DiagnosticRoutineStatusEnum::kFailed)
    return 100;
  return 0;
}

}  // namespace

const char kBatteryChargeFullDesignPath[] =
    "sys/class/power_supply/BAT0/charge_full_design";
const char kBatteryRoutineParametersInvalidMessage[] =
    "Invalid BatteryRoutineParameters.";
const char kBatteryNoChargeFullDesignMessage[] =
    "charge_full_design does not exist.";
const char kBatteryFailedReadingChargeFullDesignMessage[] =
    "Failed to read charge_full_design.";
const char kBatteryFailedParsingChargeFullDesignMessage[] =
    "Failed to parse charge_full_design.";
const char kBatteryRoutineSucceededMessage[] =
    "Battery design capacity within given limits.";
const char kBatteryRoutineFailedMessage[] =
    "Battery design capacity not within given limits.";

BatteryRoutine::BatteryRoutine(uint32_t low_mah, uint32_t high_mah)
    : status_(mojo_ipc::DiagnosticRoutineStatusEnum::kReady),
      low_mah_(low_mah),
      high_mah_(high_mah) {}

BatteryRoutine::~BatteryRoutine() = default;

void BatteryRoutine::Start() {
  DCHECK_EQ(status_, mojo_ipc::DiagnosticRoutineStatusEnum::kReady);
  status_ = RunBatteryRoutine();
  if (status_ != mojo_ipc::DiagnosticRoutineStatusEnum::kPassed)
    LOG(ERROR) << "Routine failed: " << status_message_;
}

// The battery test can only be started.
void BatteryRoutine::Resume() {}
void BatteryRoutine::Cancel() {}

void BatteryRoutine::PopulateStatusUpdate(mojo_ipc::RoutineUpdate* response,
                                          bool include_output) {
  // Because the battery routine is non-interactive, we will never include a
  // user message.
  mojo_ipc::NonInteractiveRoutineUpdate update;
  update.status = status_;
  update.status_message = status_message_;

  response->routine_update_union->set_noninteractive_update(update.Clone());
  response->progress_percent = CalculateProgressPercent(status_);
}

mojo_ipc::DiagnosticRoutineStatusEnum BatteryRoutine::GetStatus() {
  return status_;
}

void BatteryRoutine::set_root_dir_for_testing(const base::FilePath& root_dir) {
  root_dir_ = root_dir;
}

mojo_ipc::DiagnosticRoutineStatusEnum BatteryRoutine::RunBatteryRoutine() {
  if (low_mah_ > high_mah_) {
    status_message_ = kBatteryRoutineParametersInvalidMessage;
    return mojo_ipc::DiagnosticRoutineStatusEnum::kError;
  }

  base::FilePath charge_full_design_path(
      root_dir_.AppendASCII(kBatteryChargeFullDesignPath));

  if (!base::PathExists(charge_full_design_path)) {
    status_message_ = kBatteryNoChargeFullDesignMessage;
    return mojo_ipc::DiagnosticRoutineStatusEnum::kError;
  }

  std::string charge_full_design_contents;
  if (!base::ReadFileToString(charge_full_design_path,
                              &charge_full_design_contents)) {
    status_message_ = kBatteryFailedReadingChargeFullDesignMessage;
    return mojo_ipc::DiagnosticRoutineStatusEnum::kError;
  }

  base::TrimWhitespaceASCII(charge_full_design_contents, base::TRIM_TRAILING,
                            &charge_full_design_contents);
  uint32_t charge_full_design_uah;
  if (!base::StringToUint(charge_full_design_contents,
                          &charge_full_design_uah)) {
    status_message_ = kBatteryFailedParsingChargeFullDesignMessage;
    return mojo_ipc::DiagnosticRoutineStatusEnum::kError;
  }

  // Conversion is necessary because the inputs are given in mAh, whereas the
  // design capacity is reported in uAh.
  uint32_t charge_full_design_mah = charge_full_design_uah / kuAhTomAhDivisor;
  if (!(charge_full_design_mah >= low_mah_) ||
      !(charge_full_design_mah <= high_mah_)) {
    status_message_ = kBatteryRoutineFailedMessage;
    return mojo_ipc::DiagnosticRoutineStatusEnum::kFailed;
  }

  status_message_ = kBatteryRoutineSucceededMessage;
  return mojo_ipc::DiagnosticRoutineStatusEnum::kPassed;
}

}  // namespace diagnostics
