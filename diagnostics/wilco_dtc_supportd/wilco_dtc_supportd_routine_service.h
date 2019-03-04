// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_ROUTINE_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_ROUTINE_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>

#include "diagnostics/routines/diag_routine.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_routine_factory.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_routine_factory_impl.h"
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// The routine service is responsible for creating and managing diagnostic
// routines.
class WilcoDtcSupportdRoutineService final {
 public:
  using GetAvailableRoutinesToServiceCallback = base::Callback<void(
      const std::vector<grpc_api::DiagnosticRoutine>& routines)>;
  using RunRoutineToServiceCallback =
      base::Callback<void(int uuid, grpc_api::DiagnosticRoutineStatus status)>;
  using GetRoutineUpdateRequestToServiceCallback =
      base::Callback<void(int uuid,
                          grpc_api::DiagnosticRoutineStatus status,
                          int progress_percent,
                          grpc_api::DiagnosticRoutineUserMessage user_message,
                          const std::string& output)>;

  WilcoDtcSupportdRoutineService();
  explicit WilcoDtcSupportdRoutineService(
      WilcoDtcSupportdRoutineFactory* routine_factory);
  ~WilcoDtcSupportdRoutineService();

  void GetAvailableRoutines(
      const GetAvailableRoutinesToServiceCallback& callback);
  void SetAvailableRoutinesForTesting(
      const std::vector<grpc_api::DiagnosticRoutine>& available_routines);
  void RunRoutine(const grpc_api::RunRoutineRequest& request,
                  const RunRoutineToServiceCallback& callback);
  void GetRoutineUpdate(
      int uuid,
      grpc_api::GetRoutineUpdateRequest::Command command,
      bool include_output,
      const GetRoutineUpdateRequestToServiceCallback& callback);

 private:
  std::unique_ptr<WilcoDtcSupportdRoutineFactoryImpl> routine_factory_impl_;
  WilcoDtcSupportdRoutineFactory* routine_factory_;
  // Map from uuids to instances of diagnostics routines that have
  // been started.
  std::map<int, std::unique_ptr<DiagnosticRoutine>> active_routines_;
  // Generator for uuids - currently, when we need a new uuids we
  // just return next_id_, then increment next_id_.
  int next_uuid_ = 1;
  std::vector<grpc_api::DiagnosticRoutine> available_routines_{
      grpc_api::ROUTINE_BATTERY, grpc_api::ROUTINE_URANDOM};

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdRoutineService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_ROUTINE_SERVICE_H_
