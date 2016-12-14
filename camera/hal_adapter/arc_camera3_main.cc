/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <dlfcn.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <brillo/daemons/daemon.h>
#include <brillo/syslog_logging.h>

#include "arc/common.h"
#include "hal_adapter/arc_camera3_service_provider.h"
#include "hal_adapter/camera_hal_adapter.h"
#include "hardware/hardware.h"

const char kCameraHalDllName[] = "camera_hal.so";

// A closure to make sure the quit callback function is called on the main
// thread of the daemon.
static void DaemonQuitCallback(base::MessageLoop* message_loop,
                               const base::Closure& quit_callback) {
  message_loop->PostTask(FROM_HERE, quit_callback);
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

  // ArcCamera3ServiceProvider.Start() waits connection from container
  // forever.
  // Once provider accepted a connection, it forks a child process and returns
  // the fd. CameraHalAdapter uses this fd to communicate with container.
  LOGF(INFO) << "Starting ARC camera3 service provider";
  arc::ArcCamera3ServiceProvider provider;
  int fd = provider.Start();
  if (fd < 0) {
    LOGF(ERROR) << "Start ARC camera3 service failed";
    return 1;
  }

  void* camera_hal_handle = dlopen(kCameraHalDllName, RTLD_NOW);
  if (!camera_hal_handle) {
    LOGF(ERROR) << "Failed to dlopen: " << dlerror();
    return 1;
  }

  camera_module_t* camera_module = static_cast<camera_module_t*>(
      dlsym(camera_hal_handle, HAL_MODULE_INFO_SYM_AS_STR));
  if (!camera_module) {
    LOGF(ERROR) << "Failed to get camera_module_t pointer with symbol name "
                << HAL_MODULE_INFO_SYM_AS_STR;
    return 1;
  }

  VLOGF(1) << "Running camera HAL adapter on " << getpid();
  brillo::Daemon daemon;
  arc::CameraHalAdapter camera_hal_adapter(
      camera_module, fd,
      base::Bind(&DaemonQuitCallback,
                 base::Unretained(base::MessageLoop::current()),
                 base::Bind(&brillo::Daemon::Quit, base::Unretained(&daemon))));
  LOG_ASSERT(camera_hal_adapter.Start());
  daemon.Run();

  dlclose(camera_hal_handle);

  return 0;
}
