// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_daemon.h"

#include <sysexits.h>

#include <base/logging.h>
#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>
#include <dbus/diagnosticsd/dbus-constants.h>
#include <mojo/edk/embedder/embedder.h>

namespace diagnostics {

DiagnosticsdDaemon::DiagnosticsdDaemon()
    : DBusServiceDaemon(kDiagnosticsdServiceName /* service_name */) {}

DiagnosticsdDaemon::~DiagnosticsdDaemon() = default;

int DiagnosticsdDaemon::OnInit() {
  VLOG(0) << "Starting";
  const int exit_code = DBusServiceDaemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;
  // Init the Mojo Embedder API. The call to InitIPCSupport() is balanced with
  // the ShutdownIPCSupport() one in OnShutdown().
  mojo::edk::Init();
  mojo::edk::InitIPCSupport(
      base::ThreadTaskRunnerHandle::Get() /* io_thread_task_runner */);
  return EX_OK;
}

void DiagnosticsdDaemon::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  DCHECK(bus_);
  diagnosticsd_core_.RegisterDBusObjectsAsync(bus_, sequencer);
}

void DiagnosticsdDaemon::OnShutdown(int* error_code) {
  // Gracefully tear down the Mojo Embedder API.
  VLOG(1) << "Tearing down Mojo";
  base::RunLoop run_loop;
  mojo::edk::ShutdownIPCSupport(run_loop.QuitClosure());
  run_loop.Run();

  VLOG(0) << "Shutting down";
}

}  // namespace diagnostics
