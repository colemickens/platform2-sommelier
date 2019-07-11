// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_

#include <memory>

#include <base/callback.h>
#include <base/macros.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "diagnostics/cros_healthd/cros_healthd_routine_service.h"
#include "diagnostics/cros_healthd/cros_healthd_telemetry_service.h"
#include "mojo/cros_healthd.mojom.h"

namespace diagnostics {

// Implements the "CrosHealthdService" Mojo interface exposed by the
// cros_healthd daemon (see the API definition at mojo/cros_healthd.mojom)
class CrosHealthdMojoService final
    : public chromeos::cros_healthd::mojom::CrosHealthdService {
 public:
  // |routine_service_| - Unowned pointer; must outlive this instance.
  // |telemetry_service_| - Unowned pointer; must outlive this instance.
  // |mojo_pipe_handle| - Pipe to bind this instance to.
  CrosHealthdMojoService(CrosHealthdRoutineService* routine_service,
                         CrosHealthdTelemetryService* telemetry_service,
                         mojo::ScopedMessagePipeHandle mojo_pipe_handle);
  ~CrosHealthdMojoService() override;

  // chromeos::cros_healthd::mojom::CrosHealthdService overrides:
  void GetTelemetryItem(chromeos::cros_healthd::mojom::TelemetryItemEnum item,
                        const GetTelemetryItemCallback& callback) override;
  void GetTelemetryGroup(
      chromeos::cros_healthd::mojom::TelemetryGroupEnum group,
      const GetTelemetryGroupCallback& callback) override;
  void GetAvailableRoutines(
      const GetAvailableRoutinesCallback& callback) override;
  void RunRoutine(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum routine,
                  chromeos::cros_healthd::mojom::RoutineParametersPtr params,
                  const RunRoutineCallback& callback) override;
  void GetRoutineUpdate(
      int32_t uuid,
      chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum command,
      bool include_output,
      const GetRoutineUpdateCallback& callback) override;

  void ProbeBatteryInfo(const ProbeBatteryInfoCallback& callback) override;
  void ProbeNonRemovableBlockDeviceInfo(
      const ProbeNonRemovableBlockDeviceInfoCallback& callback) override;

  // Set the function that will be called if the binding encounters a connection
  // error.
  void set_connection_error_handler(const base::Closure& error_handler);

 private:
  // Unowned. The routine service should outlive this instance.
  CrosHealthdRoutineService* const routine_service_;
  // Unowned. The telemetry service should outlive this instance.
  CrosHealthdTelemetryService* const telemetry_service_;

  // Mojo binding that connects |this| with the message pipe, allowing the
  // remote end to call our methods.
  mojo::Binding<chromeos::cros_healthd::mojom::CrosHealthdService>
      self_binding_;

  DISALLOW_COPY_AND_ASSIGN(CrosHealthdMojoService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_
