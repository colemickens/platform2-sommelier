// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_ROUTINES_BATTERY_SYSFS_BATTERY_SYSFS_H_
#define DIAGNOSTICS_ROUTINES_BATTERY_SYSFS_BATTERY_SYSFS_H_

#include <map>
#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "diagnostics/routines/diag_routine.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {
// Relative path to the directory with files read by the BatterySysfs routine.
extern const char kBatterySysfsPath[];
// Paths relative to |kBatterySysfsPath| to individual files read by the
// BatterySysfs routine.
extern const char kBatterySysfsChargeFullPath[];
extern const char kBatterySysfsChargeFullDesignPath[];
extern const char kBatterySysfsEnergyFullPath[];
extern const char kBatterySysfsEnergyFullDesignPath[];
extern const char kBatterySysfsCycleCountPath[];
extern const char kBatterySysfsManufacturerPath[];
extern const char kBatterySysfsCurrentNowPath[];
extern const char kBatterySysfsPresentPath[];
extern const char kBatterySysfsStatusPath[];
extern const char kBatterySysfsVoltageNowPath[];
extern const char kBatterySysfsChargeNowPath[];

// Status messages for the BatterySysfs routine when in various states.
extern const char kBatterySysfsFailedCalculatingWearPercentageMessage[];
extern const char kBatterySysfsExcessiveWearMessage[];
extern const char kBatterySysfsFailedReadingCycleCountMessage[];
extern const char kBatterySysfsExcessiveCycleCountMessage[];
extern const char kBatterySysfsRoutinePassedMessage[];

// The battery routine checks whether or not the battery's design capacity is
// within the given limits. It reads the design capacity from the file
// kBatteryChargeFullDesignPath.
class BatterySysfsRoutine final : public DiagnosticRoutine {
 public:
  explicit BatterySysfsRoutine(
      const grpc_api::BatterySysfsRoutineParameters& parameters);

  // DiagnosticRoutine overrides:
  ~BatterySysfsRoutine() override;
  void Start() override;
  void Pause() override;
  void Resume() override;
  void Cancel() override;
  void PopulateStatusUpdate(grpc_api::GetRoutineUpdateResponse* response,
                            bool include_output) override;
  grpc_api::DiagnosticRoutineStatus GetStatus() override;

  // Overrides the file system root directory for file operations in tests.
  // If used, this function needs to be called before Start().
  void set_root_dir_for_testing(const base::FilePath& root_dir);

 private:
  bool RunBatterySysfsRoutine();
  bool ReadBatteryCapacities(int* capacity, int* design_capacity);
  bool ReadCycleCount(int* cycle_count);
  bool TestWearPercentage();
  bool TestCycleCount();

  grpc_api::DiagnosticRoutineStatus status_;
  const grpc_api::BatterySysfsRoutineParameters parameters_;
  std::map<std::string, std::string> battery_sysfs_log_;
  std::string status_message_;
  base::FilePath root_dir_{"/"};

  DISALLOW_COPY_AND_ASSIGN(BatterySysfsRoutine);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_BATTERY_SYSFS_BATTERY_SYSFS_H_
