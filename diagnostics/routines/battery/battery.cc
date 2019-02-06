// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/battery/battery.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

namespace diagnostics {

namespace {
// When the battery routine's low_mah and high_mah parameters are not set, we
// default to low_mah = 1000 and high_mah = 10000.
constexpr int kDefaultLowmAh = 1000;
constexpr int kDefaultHighmAh = 10000;
// Conversion factor from uAh to mAh.
constexpr int kuAhTomAhDivisor = 1000;

int CalculateProgressPercent(grpc_api::DiagnosticRoutineStatus status) {
  // Since the battery test cannot be paused or cancelled, the progress
  // percent can only be 0 or 100.
  if (status == grpc_api::ROUTINE_STATUS_PASSED ||
      status == grpc_api::ROUTINE_STATUS_FAILED)
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

BatteryRoutine::BatteryRoutine(
    const grpc_api::BatteryRoutineParameters& parameters)
    : status_(grpc_api::ROUTINE_STATUS_READY), parameters_(parameters) {}

BatteryRoutine::~BatteryRoutine() = default;

void BatteryRoutine::Start() {
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_READY);
  status_ = RunBatteryRoutine();
  if (status_ != grpc_api::ROUTINE_STATUS_PASSED)
    LOG(ERROR) << "Routine failed: " << output_;
}

// The battery test can only be started.
void BatteryRoutine::Pause() {}
void BatteryRoutine::Resume() {}
void BatteryRoutine::Cancel() {}

void BatteryRoutine::PopulateStatusUpdate(
    grpc_api::GetRoutineUpdateResponse* response, bool include_output) {
  // Because the battery routine is non-interactive, we will never include a
  // user message.
  response->set_status(status_);
  response->set_progress_percent(CalculateProgressPercent(status_));

  if (include_output)
    response->set_output(output_);
}

grpc_api::DiagnosticRoutineStatus BatteryRoutine::GetStatus() {
  return status_;
}

void BatteryRoutine::set_root_dir_for_testing(const base::FilePath& root_dir) {
  root_dir_ = root_dir;
}

grpc_api::DiagnosticRoutineStatus BatteryRoutine::RunBatteryRoutine() {
  int low_mah = parameters_.low_mah() ? parameters_.low_mah() : kDefaultLowmAh;
  int high_mah =
      parameters_.high_mah() ? parameters_.high_mah() : kDefaultHighmAh;

  if (low_mah > high_mah) {
    output_ = kBatteryRoutineParametersInvalidMessage;
    return grpc_api::ROUTINE_STATUS_ERROR;
  }

  base::FilePath charge_full_design_path(
      root_dir_.AppendASCII(kBatteryChargeFullDesignPath));

  if (!base::PathExists(charge_full_design_path)) {
    output_ = kBatteryNoChargeFullDesignMessage;
    return grpc_api::ROUTINE_STATUS_ERROR;
  }

  std::string charge_full_design_contents;
  if (!base::ReadFileToString(charge_full_design_path,
                              &charge_full_design_contents)) {
    output_ = kBatteryFailedReadingChargeFullDesignMessage;
    return grpc_api::ROUTINE_STATUS_ERROR;
  }

  base::TrimWhitespaceASCII(charge_full_design_contents, base::TRIM_TRAILING,
                            &charge_full_design_contents);
  int charge_full_design_uah;
  if (!base::StringToInt(charge_full_design_contents,
                         &charge_full_design_uah)) {
    output_ = kBatteryFailedParsingChargeFullDesignMessage;
    return grpc_api::ROUTINE_STATUS_ERROR;
  }

  // Conversion is necessary because the inputs are given in mAh, whereas the
  // design capacity is reported in uAh.
  int charge_full_design_mah = charge_full_design_uah / kuAhTomAhDivisor;
  if (!(charge_full_design_mah >= low_mah) ||
      !(charge_full_design_mah <= high_mah)) {
    output_ = kBatteryRoutineFailedMessage;
    return grpc_api::ROUTINE_STATUS_FAILED;
  }

  output_ = kBatteryRoutineSucceededMessage;
  return grpc_api::ROUTINE_STATUS_PASSED;
}

}  // namespace diagnostics
