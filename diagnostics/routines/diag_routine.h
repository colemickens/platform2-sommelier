// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_ROUTINES_DIAG_ROUTINE_H_
#define DIAGNOSTICS_ROUTINES_DIAG_ROUTINE_H_

#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// An interface for creating a diagnostic routine, which can be run and
// controlled by the platform.
class DiagnosticRoutine {
 public:
  virtual ~DiagnosticRoutine() = default;

  // Starts the diagnostic routine. This function should only be called a
  // single time per instance of DiagnosticRoutine.
  virtual void Start() = 0;
  // Pauses the diagnostic routine, which will be inactive until resumed.
  virtual void Pause() = 0;
  // This function should only be called to resume a routine which had
  // previously been paused.
  virtual void Resume() = 0;
  // Cancels an active diagnostics routine. Information (status, output, user
  // message) of a cancelled routine can still be accessed, but the routine
  // cannot be restarted.
  virtual void Cancel() = 0;
  // Populates |response| with the current status of the diagnostic routine.
  virtual void PopulateStatusUpdate(
      grpc_api::GetRoutineUpdateResponse* response, bool include_output) = 0;
  virtual grpc_api::DiagnosticRoutineStatus GetStatus() = 0;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_DIAG_ROUTINE_H_