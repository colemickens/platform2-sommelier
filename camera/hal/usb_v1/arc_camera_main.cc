/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/command_line.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <brillo/daemons/daemon.h>
#include <brillo/key_value_store.h>
#include <brillo/syslog_logging.h>

#include "hal/usb_v1/arc_camera_dbus_daemon.h"
#include "hal/usb_v1/arc_camera_service.h"
#include "hal/usb_v1/arc_camera_service_provider.h"

int main(int argc, char* argv[]) {
  // Init CommandLine for InitLogging.
  brillo::OpenLog("arc-camera-service", true /* log_pid */);
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  const bool kOptionPID = true;
  const bool kOptionTID = true;
  const bool kOptionTimestamp = true;
  const bool kOptionTickcount = true;
  logging::SetLogItems(kOptionPID, kOptionTID, kOptionTimestamp,
                       kOptionTickcount);

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch("child")) {
    // This process was launched in the child mode.
    std::string token = cl->GetSwitchValueASCII("child");
    base::ScopedFD fd(arc::ArcCameraDBusDaemon::kMojoChannelFD);
    brillo::Daemon daemon;
    VLOG(1) << "Starting ARC camera service";
    arc::ArcCameraServiceImpl service(
        base::Bind(&brillo::Daemon::Quit, base::Unretained(&daemon)));
    LOG_ASSERT(service.StartWithTokenAndFD(token, std::move(fd)));
    return daemon.Run();
  }

  // Run D-Bus daemon only for P.
  // TODO(hashimoto): Always run D-Bus daemon to remove the old code.
  brillo::KeyValueStore store;
  if (!store.Load(base::FilePath("/etc/lsb-release"))) {
    LOG(ERROR) << "Could not load lsb-release";
    return 1;
  }
  constexpr char kAndroidSdkVersionP[] = "28";
  std::string android_sdk_version;
  const bool shouldRunDBusDaemon =
      store.GetString("CHROMEOS_ARC_ANDROID_SDK_VERSION",
                      &android_sdk_version) &&
      android_sdk_version == kAndroidSdkVersionP;
  if (shouldRunDBusDaemon) {
    // ArcCameraDBusDaemon waits for connection from container forever.
    // Once it accepted a connection, it forks a child process and passes the
    // fd. ArcCameraService uses this fd to communicate with container.
    LOG(INFO) << "Starting ARC camera D-Bus daemon";
    arc::ArcCameraDBusDaemon dbus_daemon;
    return dbus_daemon.Run();
  }
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
      base::Bind(&brillo::Daemon::Quit, base::Unretained(&daemon)));
  LOG_ASSERT(service.StartWithSocketFD(base::ScopedFD(fd)));
  daemon.Run();

  return 0;
}
