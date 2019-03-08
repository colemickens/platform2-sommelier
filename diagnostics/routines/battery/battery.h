// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_ROUTINES_BATTERY_BATTERY_H_
#define DIAGNOSTICS_ROUTINES_BATTERY_BATTERY_H_

#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "diagnostics/routines/diag_routine.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Relative path to the charge_full_design file read by the battery routine.
extern const char kBatteryChargeFullDesignPath[];
// Output messages for the battery routine when in various states.
extern const char kBatteryRoutineParametersInvalidMessage[];
extern const char kBatteryNoChargeFullDesignMessage[];
extern const char kBatteryFailedReadingChargeFullDesignMessage[];
extern const char kBatteryFailedParsingChargeFullDesignMessage[];
extern const char kBatteryRoutineSucceededMessage[];
extern const char kBatteryRoutineFailedMessage[];

// The battery routine checks whether or not the battery's design capacity is
// within the given limits. It reads the design capacity from the file
// kBatteryChargeFullDesignPath.
class BatteryRoutine final : public DiagnosticRoutine {
 public:
  explicit BatteryRoutine(const grpc_api::BatteryRoutineParameters& parameters);

  // DiagnosticRoutine overrides:
  ~BatteryRoutine() override;
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
  grpc_api::DiagnosticRoutineStatus RunBatteryRoutine();

  grpc_api::DiagnosticRoutineStatus status_;
  const grpc_api::BatteryRoutineParameters parameters_;
  std::string output_;
  base::FilePath root_dir_{"/"};

  DISALLOW_COPY_AND_ASSIGN(BatteryRoutine);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_BATTERY_BATTERY_H_
