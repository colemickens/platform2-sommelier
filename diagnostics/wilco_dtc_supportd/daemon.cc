// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/daemon.h"

#include <cstdlib>

#include <base/callback.h>
#include <base/logging.h>
#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>
#include <dbus/wilco_dtc_supportd/dbus-constants.h>
#include <mojo/core/embedder/embedder.h>

#include "diagnostics/constants/grpc_constants.h"

namespace diagnostics {

Daemon::Daemon()
    : DBusServiceDaemon(kWilcoDtcSupportdServiceName /* service_name */),
      wilco_dtc_supportd_core_({GetWilcoDtcSupportdGrpcHostVsockUri(),
                                kWilcoDtcSupportdGrpcDomainSocketUri},
                               GetUiMessageReceiverWilcoDtcGrpcHostVsockUri(),
                               {GetWilcoDtcGrpcHostVsockUri()},
                               &wilco_dtc_supportd_core_delegate_impl_) {}

Daemon::~Daemon() = default;

int Daemon::OnInit() {
  VLOG(0) << "Starting";
  const int exit_code = DBusServiceDaemon::OnInit();
  if (exit_code != EXIT_SUCCESS)
    return exit_code;

  if (!wilco_dtc_supportd_core_.Start()) {
    LOG(ERROR) << "Shutting down due to fatal initialization failure";
    base::RunLoop run_loop;
    wilco_dtc_supportd_core_.ShutDown(run_loop.QuitClosure());
    run_loop.Run();
    return EXIT_FAILURE;
  }

  // Init the Mojo Embedder API. The call to InitIPCSupport() is balanced with
  // the ShutdownIPCSupport() one in OnShutdown().
  mojo::core::Init();
  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      base::ThreadTaskRunnerHandle::Get() /* io_thread_task_runner */,
      mojo::core::ScopedIPCSupport::ShutdownPolicy::
          CLEAN /* blocking shutdown */);

  return EXIT_SUCCESS;
}

void Daemon::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  DCHECK(bus_);
  wilco_dtc_supportd_core_.RegisterDBusObjectsAsync(bus_, sequencer);
}

void Daemon::OnShutdown(int* error_code) {
  // Gracefully tear down pieces that require asynchronous shutdown.
  VLOG(1) << "Shutting down";

  base::RunLoop run_loop;
  wilco_dtc_supportd_core_.ShutDown(run_loop.QuitClosure());
  run_loop.Run();

  VLOG(0) << "Shutting down with code " << *error_code;
}

}  // namespace diagnostics
