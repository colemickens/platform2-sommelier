// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_ROUTINES_URANDOM_URANDOM_H_
#define DIAGNOSTICS_ROUTINES_URANDOM_URANDOM_H_

#include <cstdint>
#include <memory>
#include <string>

#include <base/macros.h>
#include <base/process/process.h>
#include <base/time/tick_clock.h>
#include <base/time/time.h>

#include "diagnostics/routines/diag_process_adapter.h"
#include "diagnostics/routines/diag_routine.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Output messages for the urandom routine when in various states.
extern const char kUrandomProcessRunningMessage[];
extern const char kUrandomRoutineSucceededMessage[];
extern const char kUrandomRoutineFailedMessage[];
extern const char kUrandomFailedToLaunchProcessMessage[];
extern const char kUrandomProcessCrashedOrKilledMessage[];
extern const char kUrandomFailedToPauseMessage[];
extern const char kUrandomFailedToCancelMessage[];
extern const char kUrandomFailedToStopMessage[];
extern const char kUrandomInvalidParametersMessage[];
// Progress percentage when the urandom routine has been running for equal to
// or longer than the time specified in the UrandomRoutineParameters, but has
// not stopped yet.
extern const int64_t kUrandomLongRunningProgress;

// The urandom routine takes a single parameter, which is the number of seconds
// to run the routine. It continuously reads 1MiB of data from /dev/urandom for
// the specified number of seconds. If at any point it fails to read 1MiB, the
// test fails.
class UrandomRoutine final : public DiagnosticRoutine {
 public:
  explicit UrandomRoutine(const grpc_api::UrandomRoutineParameters& parameters);
  UrandomRoutine(const grpc_api::UrandomRoutineParameters& parameters,
                 std::unique_ptr<base::TickClock> tick_clock_impl,
                 std::unique_ptr<DiagProcessAdapter> process_adapter);

  // DiagnosticRoutine overrides:
  ~UrandomRoutine() override;
  void Start() override;
  void Pause() override;
  void Resume() override;
  void Cancel() override;
  void PopulateStatusUpdate(grpc_api::GetRoutineUpdateResponse* response,
                            bool include_output) override;
  grpc_api::DiagnosticRoutineStatus GetStatus() override;

 private:
  int CalculateProgressPercent();

  // Functions to manipulate the urandom child process.
  void StartProcess();
  bool KillProcess(const std::string& failure_message);
  void CheckProcessStatus();

  grpc_api::DiagnosticRoutineStatus status_;
  grpc_api::UrandomRoutineParameters parameters_;
  std::string output_;
  std::unique_ptr<DiagProcessAdapter> process_adapter_;
  std::unique_ptr<base::TickClock> tick_clock_;
  base::TimeTicks start_ticks_;
  base::TimeDelta elapsed_time_;
  base::ProcessHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(UrandomRoutine);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_URANDOM_URANDOM_H_
