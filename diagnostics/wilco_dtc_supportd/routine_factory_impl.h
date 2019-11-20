// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_ROUTINE_FACTORY_IMPL_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_ROUTINE_FACTORY_IMPL_H_

#include <memory>

#include <base/macros.h>

#include "diagnostics/wilco_dtc_supportd/routine_factory.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Production implementation of RoutineFactory.
class RoutineFactoryImpl final : public RoutineFactory {
 public:
  RoutineFactoryImpl();
  ~RoutineFactoryImpl() override;

  std::unique_ptr<DiagnosticRoutine> CreateRoutine(
      const grpc_api::RunRoutineRequest& request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RoutineFactoryImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_ROUTINE_FACTORY_IMPL_H_
