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

#include "arc/common.h"
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

  while (true) {
    pid_t pid = fork();

    // Start the CameraHalServerImpl on the child process.  The process
    // will exit on error.  The main process will respawn the child process
    // to resurrect CameraHalServerImpl.
    if (!pid) {
      // Child process: Starts ARC camera3 service provider which will host the
      // camera HAL adapter.
      LOGF(INFO) << "Starting ARC camera3 service provider...";

      // Create the daemon instance first to properly set up MessageLoop and
      // AtExitManager.
      brillo::Daemon daemon;

      arc::CameraHalServerImpl service_provider;
      if (!service_provider.Start()) {
        LOGF(ERROR) << "Failed to start ARC camera3 service provider";
        return ECANCELED;
      }

      // The child process runs until an error happens which will terminate the
      // process.
      daemon.Run();
      LOGF(ERROR) << "daemon stopped";
      return 0;
    } else if (pid > 0) {
      // Parent process: Waits until child process exits, and respawns a new
      // child process.

      // Blocks until child process exits.
      int wstatus;
      wait(&wstatus);
      LOGF(INFO) << "Child exited: status=" << WEXITSTATUS(wstatus);

#if !defined(NDEBUG)
      // For debug build we respawn the service provider only when the Mojo
      // connection is aborted by remote (i.e. the Chrome ArcCamera3Service);
      // in case the HAL crashes, we stop respawning the service provider.
      // This helps us identify potential bugs in HAL more easily.
      //
      // For release builds we always respawn the service provider.
      //
      // Known issue: On debug builds, `stop ui` or browser process crash will
      // CHECK inside libmojo due to a race condition and stop the service
      // provider.  This is not an issue for release builds as we will always
      // respawn the child process to resurrect the service provider.
      if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != ECONNRESET) {
        LOGF(ERROR) << "Unexpected error killed the service provider process";
        break;
      }
#endif
    } else {
      PLOGF(ERROR) << "fork() failed";
    }
  }
  return 0;
}
