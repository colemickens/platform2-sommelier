// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_ROUTINES_SMARTCTL_CHECK_SMARTCTL_CHECK_H_
#define DIAGNOSTICS_ROUTINES_SMARTCTL_CHECK_SMARTCTL_CHECK_H_

#include <memory>

#include "diagnostics/routines/subproc_routine.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

std::unique_ptr<DiagnosticRoutine> CreateSmartctlCheckRoutine(
    const grpc_api::SmartctlCheckRoutineParameters& parameters);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_SMARTCTL_CHECK_SMARTCTL_CHECK_H_
