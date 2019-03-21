// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/smartctl_check/smartctl_check.h"

#include <utility>

#include <base/logging.h>
#include <base/process/process_handle.h>
#include <base/command_line.h>

#include "diagnostics/routines/diag_process_adapter_impl.h"

namespace diagnostics {

SmartctlCheckRoutine::SmartctlCheckRoutine(
    const grpc_api::SmartctlCheckRoutineParameters& parameters)
    : SmartctlCheckRoutine(parameters,
                           std::make_unique<DiagProcessAdapterImpl>()) {}

SmartctlCheckRoutine::SmartctlCheckRoutine(
    const grpc_api::SmartctlCheckRoutineParameters& parameters,
    std::unique_ptr<DiagProcessAdapter> process_adapter)
    : SubprocRoutine(std::move(process_adapter)), parameters_(parameters) {}

SmartctlCheckRoutine::~SmartctlCheckRoutine() = default;

base::CommandLine SmartctlCheckRoutine::GetCommandLine() const {
  return base::CommandLine({kSmartctlCheckExecutableInstallLocation});
}

}  // namespace diagnostics
