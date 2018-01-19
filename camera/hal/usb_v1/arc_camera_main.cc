/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <base/bind.h>
#include <base/command_line.h>
#include <brillo/daemons/daemon.h>
#include <brillo/syslog_logging.h>

#include "hal/usb_v1/arc_camera_service.h"
#include "hal/usb_v1/arc_camera_service_provider.h"

int main(int argc, char* argv[]) {
  // Init CommandLine for InitLogging.
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  int log_flags = brillo::kLogToSyslog;
  if (cl->HasSwitch("foreground")) {
    log_flags |= brillo::kLogToStderr;
  }
  brillo::InitLog(log_flags);

  // ArcCameraServiceProvider.Start() waits connection from container forever.
  // Once provider accepted a connection, it forks a child process and returns
  // the fd. ArcCameraService uses this fd to communicate with container.
  LOG(INFO) << "Starting ARC camera service provider";
  arc::ArcCameraServiceProvider provider;
  int fd = provider.Start();

  if (fd < 0) {
    LOG(ERROR) << "Start ARC camera service failed";
    return 1;
  }
  brillo::Daemon daemon;
  VLOG(1) << "Starting ARC camera service";
  arc::ArcCameraServiceImpl service(
      fd, base::Bind(&brillo::Daemon::Quit, base::Unretained(&daemon)));
  LOG_ASSERT(service.Start());
  daemon.Run();

  return 0;
}
