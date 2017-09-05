// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// hammerd - A daemon to update the firmware of Hammer

#include <stdlib.h>

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "hammerd/hammer_updater.h"
#include "hammerd/process_lock.h"

namespace {
// The lock file used to prevent multiple hammerd be invoked at the same time.
constexpr char kLockFile[] = "/run/lock/hammerd.lock";
}  // namespace

int main(int argc, const char* argv[]) {
  // hammerd should be triggered by upstart job.
  // The default value of arguments are stored in `/etc/init/hammerd.conf`, and
  // each board should override values in `/etc/init/hammerd.override`.
  DEFINE_string(ec_image_path, "", "Path to the EC firmware image file");
  DEFINE_string(touchpad_image_path, "", "Path to the touchpad image file");
  // TODO(b/65534217): Define a flag about touchpad version that is expected
  //                   to be computed by init script.
  DEFINE_int32(vendor_id, -1, "USB vendor ID of the device");
  DEFINE_int32(product_id, -1, "USB product ID of the device");
  DEFINE_int32(usb_bus, -1, "USB bus to search");
  DEFINE_int32(usb_port, -1, "USB port to search");
  DEFINE_int32(autosuspend_delay_ms, -1, "USB autosuspend delay time (ms)");
  DEFINE_bool(at_boot, false,
              "Invoke proecess at boot time. "
              "Exit if RW is up-to-date (no pairing)");
  brillo::FlagHelper::Init(argc, argv, "Hammer EC firmware updater daemon");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);

  base::FilePath file_path(FILE_PATH_LITERAL(kLockFile));
  hammerd::ProcessLock lock(file_path);
  if (!lock.Acquire()) {
    LOG(INFO) << "Other hammerd process is running, exit.";
    return EXIT_SUCCESS;
  }

  if (FLAGS_vendor_id < 0 || FLAGS_product_id < 0 ||
      FLAGS_usb_bus < 0 || FLAGS_usb_port < 0) {
    LOG(ERROR) << "Must specify USB vendor/product ID and bus/port number.";
    return EXIT_FAILURE;
  }

  std::string ec_image;
  if (!base::ReadFileToString(base::FilePath(FLAGS_ec_image_path), &ec_image)) {
    LOG(ERROR) << "EC image file is not found: " << FLAGS_ec_image_path;
    return EXIT_FAILURE;
  }

  std::string touchpad_image;
  if (!FLAGS_touchpad_image_path.size()) {
    LOG(INFO) << "Touchpad image is not assigned. " <<
                 "Proceeding without updating touchpad.";

  } else if (!base::ReadFileToString(base::FilePath(FLAGS_touchpad_image_path),
                                     &touchpad_image)) {
    LOG(ERROR) << "Touchpad image is not found with path ["
               << FLAGS_touchpad_image_path << "]. Abort.";
    return EXIT_FAILURE;
  }

  // The message loop registers a task runner with the current thread, which
  // is used by DBusWrapper to send signals.
  base::MessageLoop message_loop;
  hammerd::HammerUpdater updater(
      ec_image, touchpad_image, FLAGS_vendor_id, FLAGS_product_id,
      FLAGS_usb_bus, FLAGS_usb_port, FLAGS_at_boot);
  bool ret = updater.Run();
  if (ret && FLAGS_autosuspend_delay_ms >= 0) {
    LOG(INFO) << "Enable USB autosuspend with delay "
              << FLAGS_autosuspend_delay_ms << " ms.";
    base::FilePath base_path =
        hammerd::GetUsbSysfsPath(FLAGS_usb_bus, FLAGS_usb_port);
    constexpr char kPowerLevelPath[] = "power/level";
    constexpr char kAutosuspendDelayMsPath[] = "power/autosuspend_delay_ms";
    constexpr char kPowerLevel[] = "auto";
    std::string delay_ms = base::StringPrintf("%d", FLAGS_autosuspend_delay_ms);

    base::WriteFile(base_path.Append(base::FilePath(kPowerLevelPath)),
                    kPowerLevel, sizeof(kPowerLevel) - 1);
    base::WriteFile(base_path.Append(base::FilePath(kAutosuspendDelayMsPath)),
                    delay_ms.data(), delay_ms.size());
  }
  return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
