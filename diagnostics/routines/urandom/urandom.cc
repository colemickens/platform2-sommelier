// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/urandom/urandom.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/process/process_handle.h>
#include <base/time/default_tick_clock.h>

#include "diagnostics/routines/diag_process_adapter_impl.h"

namespace diagnostics {

namespace {
constexpr char kUrandomExePath[] = "/usr/libexec/diagnostics/urandom";
}  // namespace

const char kUrandomProcessRunningMessage[] = "Test is still running.";
const char kUrandomRoutineSucceededMessage[] = "Test passed.";
const char kUrandomRoutineFailedMessage[] = "Test failed.";
const char kUrandomFailedToLaunchProcessMessage[] =
    "Could not launch the urandom process.";
const char kUrandomProcessCrashedOrKilledMessage[] =
    "The urandom process crashed or was killed.";
const char kUrandomFailedToPauseMessage[] = "Failed to pause urandom routine.";
const char kUrandomFailedToCancelMessage[] =
    "Failed to cancel urandom routine.";
const char kUrandomFailedToStopMessage[] = "Failed to stop urandom routine.";
const char kUrandomInvalidParametersMessage[] =
    "Invalid parameters for the urandom routine.";
const int64_t kUrandomLongRunningProgress = 99;

UrandomRoutine::UrandomRoutine(
    const grpc_api::UrandomRoutineParameters& parameters)
    : UrandomRoutine(parameters,
                     std::make_unique<base::DefaultTickClock>(),
                     std::make_unique<DiagProcessAdapterImpl>()) {}

UrandomRoutine::UrandomRoutine(
    const grpc_api::UrandomRoutineParameters& parameters,
    std::unique_ptr<base::TickClock> tick_clock_impl,
    std::unique_ptr<DiagProcessAdapter> process_adapter)
    : status_(grpc_api::ROUTINE_STATUS_READY),
      parameters_(parameters),
      process_adapter_(std::move(process_adapter)),
      tick_clock_(std::move(tick_clock_impl)) {}

UrandomRoutine::~UrandomRoutine() {
  // If the routine is still running, make sure to stop it so we aren't left
  // with a zombie process.
  if (status_ == grpc_api::ROUTINE_STATUS_RUNNING)
    KillProcess(kUrandomFailedToStopMessage);
}

void UrandomRoutine::Start() {
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_READY);
  if (parameters_.length_seconds() == 0) {
    status_ = grpc_api::ROUTINE_STATUS_PASSED;
    status_message_ = kUrandomRoutineSucceededMessage;
    return;
  }
  if (parameters_.length_seconds() < 0) {
    status_ = grpc_api::ROUTINE_STATUS_ERROR;
    status_message_ = kUrandomInvalidParametersMessage;
    LOG(ERROR) << kUrandomInvalidParametersMessage;
    return;
  }
  StartProcess();
}

void UrandomRoutine::Pause() {
  // Kill the current process, but record the completion percentage.
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_RUNNING);
  if (KillProcess(kUrandomFailedToPauseMessage))
    status_ = grpc_api::ROUTINE_STATUS_READY;
}

void UrandomRoutine::Resume() {
  // Call the urandom program again, with a new calculated timeout based on the
  // completion percentage and current time.
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_READY);
  StartProcess();
}

void UrandomRoutine::Cancel() {
  // Kill the process and record the completion percentage.
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_RUNNING);
  if (KillProcess(kUrandomFailedToCancelMessage))
    status_ = grpc_api::ROUTINE_STATUS_CANCELLED;
}

void UrandomRoutine::PopulateStatusUpdate(
    grpc_api::GetRoutineUpdateResponse* response, bool include_output) {
  // Because the urandom routine is non-interactive, we will never include a
  // user message.
  if (status_ == grpc_api::ROUTINE_STATUS_RUNNING)
    CheckProcessStatus();

  response->set_status(status_);
  response->set_progress_percent(CalculateProgressPercent());
  response->set_status_message(status_message_);
}

grpc_api::DiagnosticRoutineStatus UrandomRoutine::GetStatus() {
  return status_;
}

int UrandomRoutine::CalculateProgressPercent() {
  if (status_ == grpc_api::ROUTINE_STATUS_PASSED ||
      status_ == grpc_api::ROUTINE_STATUS_FAILED ||
      status_ == grpc_api::ROUTINE_STATUS_ERROR)
    return 100;

  if (status_ == grpc_api::ROUTINE_STATUS_RUNNING) {
    base::TimeTicks now_ticks = tick_clock_->NowTicks();
    elapsed_time_ += now_ticks - start_ticks_;
    start_ticks_ = now_ticks;
  }

  return std::min(
      kUrandomLongRunningProgress,
      100 * elapsed_time_ /
          base::TimeDelta::FromSeconds(parameters_.length_seconds()));
}

void UrandomRoutine::StartProcess() {
  status_ = grpc_api::ROUTINE_STATUS_RUNNING;
  start_ticks_ = tick_clock_->NowTicks();

  std::string time_delta_ms_value = std::to_string(
      (base::TimeDelta::FromSeconds(parameters_.length_seconds()) -
       elapsed_time_)
          .InMilliseconds());
  if (!process_adapter_->StartProcess(
          std::vector<std::string>{kUrandomExePath,
                                   "--time_delta_ms=" + time_delta_ms_value,
                                   "--urandom_path=/dev/urandom"},
          &handle_)) {
    status_ = grpc_api::ROUTINE_STATUS_ERROR;
    status_message_ = kUrandomFailedToLaunchProcessMessage;
    LOG(ERROR) << kUrandomFailedToLaunchProcessMessage;
  }
}

bool UrandomRoutine::KillProcess(const std::string& failure_message) {
  DCHECK_NE(handle_, base::kNullProcessHandle);
  // If the process has already exited, there's no need to kill it.
  CheckProcessStatus();

  if (status_ != grpc_api::ROUTINE_STATUS_RUNNING)
    return false;

  if (!process_adapter_->KillProcess(handle_)) {
    // Reset the ProcessHandle, so we can't accidentally kill another process
    // which has reused our old PID.
    handle_ = base::kNullProcessHandle;
    status_ = grpc_api::ROUTINE_STATUS_ERROR;
    status_message_ = failure_message;
    LOG(ERROR) << "Failed to kill process.";
    return false;
  }

  elapsed_time_ += tick_clock_->NowTicks() - start_ticks_;
  return true;
}

void UrandomRoutine::CheckProcessStatus() {
  DCHECK_NE(handle_, base::kNullProcessHandle);
  switch (process_adapter_->GetStatus(handle_)) {
    case base::TERMINATION_STATUS_STILL_RUNNING:
      status_message_ = kUrandomProcessRunningMessage;
      break;
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      handle_ = base::kNullProcessHandle;
      status_ = grpc_api::ROUTINE_STATUS_PASSED;
      status_message_ = kUrandomRoutineSucceededMessage;
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      handle_ = base::kNullProcessHandle;
      status_ = grpc_api::ROUTINE_STATUS_FAILED;
      status_message_ = kUrandomRoutineFailedMessage;
      break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      handle_ = base::kNullProcessHandle;
      status_ = grpc_api::ROUTINE_STATUS_ERROR;
      status_message_ = kUrandomFailedToLaunchProcessMessage;
      break;
    default:
      handle_ = base::kNullProcessHandle;
      status_ = grpc_api::ROUTINE_STATUS_ERROR;
      status_message_ = kUrandomProcessCrashedOrKilledMessage;
      break;
  }
}

}  // namespace diagnostics
