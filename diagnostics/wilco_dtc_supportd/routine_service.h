// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_ROUTINE_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_ROUTINE_SERVICE_H_

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>

#include "mojo/cros_healthd.mojom.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// The routine service is responsible for creating and managing diagnostic
// routines.
class RoutineService final {
 public:
  using GetAvailableRoutinesToServiceCallback = base::Callback<void(
      const std::vector<grpc_api::DiagnosticRoutine>& routines,
      grpc_api::RoutineServiceStatus service_status)>;
  using RunRoutineToServiceCallback =
      base::Callback<void(int uuid,
                          grpc_api::DiagnosticRoutineStatus status,
                          grpc_api::RoutineServiceStatus service_status)>;
  using GetRoutineUpdateRequestToServiceCallback =
      base::Callback<void(int uuid,
                          grpc_api::DiagnosticRoutineStatus status,
                          int progress_percent,
                          grpc_api::DiagnosticRoutineUserMessage user_message,
                          const std::string& output,
                          const std::string& status_message,
                          grpc_api::RoutineServiceStatus service_status)>;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Binds |service| to an implementation of CrosHealthdDiagnosticsService. In
    // production, the implementation is provided by cros_healthd. Returns false
    // if wilco_dtc_supportd's mojo service has not been started by Chrome at
    // the time this is called.
    virtual bool GetCrosHealthdDiagnosticsService(
        chromeos::cros_healthd::mojom::CrosHealthdDiagnosticsServiceRequest
            service) = 0;
  };

  // |delegate| - Unowned pointer; must outlive this instance.
  explicit RoutineService(Delegate* delegate);
  ~RoutineService();

  void GetAvailableRoutines(
      const GetAvailableRoutinesToServiceCallback& callback);
  void RunRoutine(const grpc_api::RunRoutineRequest& request,
                  const RunRoutineToServiceCallback& callback);
  void GetRoutineUpdate(
      int uuid,
      grpc_api::GetRoutineUpdateRequest::Command command,
      bool include_output,
      const GetRoutineUpdateRequestToServiceCallback& callback);

 private:
  // Binds |service_ptr_| to an implementation of CrosHealthdDiagnosticsService,
  // if it is not already bound. Returns false if wilco_dtc_supportd's mojo
  // service is not yet running and the binding cannot be attempted.
  bool BindCrosHealthdDiagnosticsServiceIfNeeded();
  // Disconnect handler called if the mojo connection to cros_healthd is lost.
  void OnDisconnect();

  // Unowned. Should outlive this instance.
  Delegate* delegate_ = nullptr;

  // Mojo interface to the CrosHealthdDiagnosticsService endpoint.
  //
  // In production this interface is implemented by the cros_healthd process.
  chromeos::cros_healthd::mojom::CrosHealthdDiagnosticsServicePtr service_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RoutineService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_ROUTINE_SERVICE_H_
