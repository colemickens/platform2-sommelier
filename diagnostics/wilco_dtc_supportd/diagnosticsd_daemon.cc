// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/diagnosticsd_daemon.h"

#include <cstdlib>

#include <base/callback.h>
#include <base/logging.h>
#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>
#include <dbus/diagnosticsd/dbus-constants.h>
#include <mojo/edk/embedder/embedder.h>

#include "diagnostics/constants/grpc_constants.h"
#include "diagnostics/wilco_dtc_supportd/bind_utils.h"

namespace diagnostics {

DiagnosticsdDaemon::DiagnosticsdDaemon()
    : DBusServiceDaemon(kDiagnosticsdServiceName /* service_name */),
      diagnosticsd_core_(kWilcoDtcSupportdGrpcUri,
                         kUiMessageReceiverWilcoDtcGrpcUri,
                         {kWilcoDtcGrpcUri},
                         &diagnosticsd_core_delegate_impl_) {}

DiagnosticsdDaemon::~DiagnosticsdDaemon() = default;

int DiagnosticsdDaemon::OnInit() {
  VLOG(0) << "Starting";
  const int exit_code = DBusServiceDaemon::OnInit();
  if (exit_code != EXIT_SUCCESS)
    return exit_code;

  if (!diagnosticsd_core_.Start()) {
    LOG(ERROR) << "Shutting down due to fatal initialization failure";
    base::RunLoop run_loop;
    diagnosticsd_core_.ShutDown(run_loop.QuitClosure());
    run_loop.Run();
    return EXIT_FAILURE;
  }

  // Init the Mojo Embedder API. The call to InitIPCSupport() is balanced with
  // the ShutdownIPCSupport() one in OnShutdown().
  mojo::edk::Init();
  mojo::edk::InitIPCSupport(
      base::ThreadTaskRunnerHandle::Get() /* io_thread_task_runner */);

  return EXIT_SUCCESS;
}

void DiagnosticsdDaemon::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  DCHECK(bus_);
  diagnosticsd_core_.RegisterDBusObjectsAsync(bus_, sequencer);
}

void DiagnosticsdDaemon::OnShutdown(int* error_code) {
  // Gracefully tear down pieces that require asynchronous shutdown.
  VLOG(1) << "Shutting down";

  base::RunLoop run_loop;
  const base::Closure barrier_closure =
      BarrierClosure(2, run_loop.QuitClosure());
  mojo::edk::ShutdownIPCSupport(barrier_closure);
  diagnosticsd_core_.ShutDown(barrier_closure);
  run_loop.Run();

  VLOG(0) << "Shutting down with code " << *error_code;
}

}  // namespace diagnostics
