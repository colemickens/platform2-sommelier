/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <hardware/hardware.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <brillo/daemons/daemon.h>
#include <brillo/syslog_logging.h>

#include "cros-camera/common.h"
#include "hal_adapter/camera_hal_adapter.h"
#include "hal_adapter/camera_hal_server_impl.h"

static void SetLogItems() {
  const bool kOptionPID = true;
  const bool kOptionTID = true;
  const bool kOptionTimestamp = true;
  const bool kOptionTickcount = true;
  logging::SetLogItems(kOptionPID, kOptionTID, kOptionTimestamp,
                       kOptionTickcount);
}

int main(int argc, char* argv[]) {
  // Init CommandLine for InitLogging.
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  int log_flags = brillo::kLogToSyslog;
  if (cl->HasSwitch("foreground")) {
    log_flags |= brillo::kLogToStderr;
  }
  brillo::InitLog(log_flags);
  // Override the log items set by brillo::InitLog.
  SetLogItems();

  pid_t pid = fork();

  // Start the CameraHalServerImpl on the child process.  The process
  // will exit on error.
  if (!pid) {
    // Child process: Starts Chrome OS camera service provider which will host
    // the camera HAL adapter.

    // Create the daemon instance first to properly set up MessageLoop and
    // AtExitManager.
    brillo::Daemon daemon;

    cros::CameraHalServerImpl service_provider;
    if (!service_provider.Start()) {
      LOGF(ERROR) << "Failed to start camera HAL v3 adapter";
      return ECANCELED;
    }

    // The child process runs until an error happens which will terminate the
    // process.
    LOGF(INFO) << "Started camera HAL v3 adapter";
    daemon.Run();
    LOGF(ERROR) << "daemon stopped";
    return 0;
  } else if (pid > 0) {
    // Parent process: Waits until child process exits and report exit status.

    // Blocks until child process exits.
    int wstatus;
    wait(&wstatus);
    LOGF(INFO) << "Child exited: status=" << WEXITSTATUS(wstatus);
  } else {
    PLOGF(ERROR) << "fork() failed";
    return pid;
  }
}
