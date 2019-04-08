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

#include "diagnostics/routines/diag_process_adapter.h"
#include "diagnostics/routines/diag_routine.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

std::unique_ptr<DiagnosticRoutine> CreateUrandomRoutine(
    const grpc_api::UrandomRoutineParameters& parameters);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_URANDOM_URANDOM_H_
