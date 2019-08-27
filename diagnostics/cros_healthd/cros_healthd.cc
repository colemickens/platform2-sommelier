// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/cros_healthd.h"

#include <cstdlib>
#include <memory>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>
#include <dbus/cros_healthd/dbus-constants.h>
#include <mojo/edk/embedder/embedder.h>

#include "diagnostics/cros_healthd/cros_healthd_routine_service_impl.h"

namespace diagnostics {

CrosHealthd::CrosHealthd()
    : DBusServiceDaemon(kCrosHealthdServiceName /* service_name */),
      routine_service_(std::make_unique<CrosHealthdRoutineServiceImpl>()) {}

CrosHealthd::~CrosHealthd() = default;

int CrosHealthd::OnInit() {
  VLOG(0) << "Starting";
  const int exit_code = DBusServiceDaemon::OnInit();
  if (exit_code != EXIT_SUCCESS)
    return exit_code;

  // Init the Mojo Embedder API. The call to InitIPCSupport() is balanced with
  // the ShutdownIPCSupport() one in OnShutdown().
  mojo::edk::Init();
  mojo::edk::InitIPCSupport(
      base::ThreadTaskRunnerHandle::Get() /* io_thread_task_runner */);

  return EXIT_SUCCESS;
}

void CrosHealthd::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  DCHECK(!dbus_object_);
  dbus_object_ = std::make_unique<brillo::dbus_utils::DBusObject>(
      nullptr /* object_manager */, bus_,
      dbus::ObjectPath(kCrosHealthdServicePath));
  brillo::dbus_utils::DBusInterface* dbus_interface =
      dbus_object_->AddOrGetInterface(kCrosHealthdServiceInterface);
  DCHECK(dbus_interface);
  dbus_interface->AddSimpleMethodHandlerWithError(
      kCrosHealthdBootstrapMojoConnectionMethod, base::Unretained(this),
      &CrosHealthd::BootstrapMojoConnection);
  dbus_object_->RegisterAsync(sequencer->GetHandler(
      "Failed to register D-Bus object" /* descriptive_message */,
      true /* failure_is_fatal */));
}

void CrosHealthd::OnShutdown(int* error_code) {
  // Gracefully tear down pieces that require asynchronous shutdown.
  VLOG(1) << "Starting to shut down";

  base::RunLoop run_loop;
  mojo::edk::ShutdownIPCSupport(run_loop.QuitClosure());
  run_loop.Run();

  VLOG(1) << "Finished shutting down Mojo support with code " << *error_code;

  DBusServiceDaemon::OnShutdown(error_code);
}

bool CrosHealthd::BootstrapMojoConnection(brillo::ErrorPtr* error,
                                          const base::ScopedFD& mojo_fd) {
  VLOG(1) << "Received BootstrapMojoConnection D-Bus request";

  if (!mojo_fd.is_valid()) {
    constexpr char kInvalidFileDescriptorError[] =
        "Invalid Mojo file descriptor";
    LOG(ERROR) << kInvalidFileDescriptorError;
    *error =
        brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                              DBUS_ERROR_FAILED, kInvalidFileDescriptorError);
    return false;
  }

  if (mojo_service_bind_attempted_) {
    // This should not normally be triggered, since the other endpoint - the
    // browser process - should bootstrap the Mojo connection only once, and
    // when that process is killed the Mojo shutdown notification should have
    // been received earlier, but handle this case to be on the safe side. After
    // our restart the browser process is expected to invoke the bootstrapping
    // again.
    constexpr char kRepeatedBootstrapRequest[] =
        "Repeated Mojo bootstrap request received";
    *error =
        brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                              DBUS_ERROR_FAILED, kRepeatedBootstrapRequest);
    ShutDownDueToMojoError(kRepeatedBootstrapRequest /* debug_reason */);
    return false;
  }

  // We need a file descriptor that stays alive after the current method
  // finishes, but libbrillo's D-Bus wrappers currently don't support passing
  // base::ScopedFD by value.
  base::ScopedFD mojo_fd_copy(HANDLE_EINTR(dup(mojo_fd.get())));
  if (!mojo_fd_copy.is_valid()) {
    constexpr char kFailedDuplicationError[] =
        "Failed to duplicate the Mojo file descriptor";
    PLOG(ERROR) << kFailedDuplicationError;
    *error = brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                                   DBUS_ERROR_FAILED, kFailedDuplicationError);
    return false;
  }

  if (!base::SetCloseOnExec(mojo_fd_copy.get())) {
    constexpr char kFailedSettingFdCloexec[] =
        "Failed to set FD_CLOEXEC on Mojo file descriptor";
    PLOG(ERROR) << kFailedSettingFdCloexec;
    *error = brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                                   DBUS_ERROR_FAILED, kFailedSettingFdCloexec);
    return false;
  }

  // Connect to mojo in the requesting process.
  mojo::edk::SetParentPipeHandle(mojo::edk::ScopedPlatformHandle(
      mojo::edk::PlatformHandle(mojo_fd_copy.release())));

  mojo_service_bind_attempted_ = true;
  mojo_service_ = std::make_unique<CrosHealthdMojoService>(
      routine_service_.get(), mojo::edk::CreateChildMessagePipe(
                                  kCrosHealthdMojoConnectionChannelToken));
  if (!mojo_service_) {
    constexpr char kBootstrapFailed[] = "Mojo bootstrap failed";
    *error = brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                                   DBUS_ERROR_FAILED, kBootstrapFailed);
    ShutDownDueToMojoError(kBootstrapFailed /* debug_reason */);
    return false;
  }
  mojo_service_->set_connection_error_handler(
      base::Bind(&CrosHealthd::ShutDownDueToMojoError, base::Unretained(this),
                 "Mojo connection error" /* debug_reason */));
  VLOG(1) << "Successfully bootstrapped Mojo connection";
  return true;
}

void CrosHealthd::ShutDownDueToMojoError(const std::string& debug_reason) {
  // Our daemon has to be restarted to be prepared for future Mojo connection
  // bootstraps. We can't do this without a restart since Mojo EDK gives no
  // guarantees it will support repeated bootstraps. Therefore, tear down and
  // exit from our process and let upstart restart us again.
  LOG(ERROR) << "Shutting down due to: " << debug_reason;
  mojo_service_.reset();
  Quit();
}

}  // namespace diagnostics
