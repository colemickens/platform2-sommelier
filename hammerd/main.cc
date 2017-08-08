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
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "hammerd/hammer_updater.h"
#include "hammerd/process_lock.h"

namespace {
// The lock file used to prevent multiple hammerd be invoked at the same time.
constexpr char kLockFile[] = "/run/lock/hammerd.lock";
// Use Google as the default USB vendor ID.
constexpr uint16_t kDefaultUsbVendorId = 0x18d1;
// TODO(kitching): Remove these defaults after merging new udev rules.
constexpr char kDefaultImagePath[] = "/lib/firmware/hammer.fw";
constexpr uint16_t kDefaultUsbProductId = 0x5022;
constexpr int kDefaultUsbBus = 1;
constexpr int kDefaultUsbPort = 2;
}  // namespace

int main(int argc, const char* argv[]) {
  DEFINE_string(image_path, kDefaultImagePath,
                "Path to the firmware image file");
  DEFINE_int32(vendor_id, kDefaultUsbVendorId,
               "USB vendor ID of the device");
  DEFINE_int32(product_id, kDefaultUsbProductId,
               "USB product ID of the device");
  DEFINE_int32(usb_bus, kDefaultUsbBus, "USB bus to search");
  DEFINE_int32(usb_port, kDefaultUsbPort, "USB port to search");
  brillo::FlagHelper::Init(argc, argv, "Hammer EC firmware updater daemon");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);

  base::FilePath file_path(FILE_PATH_LITERAL(kLockFile));
  hammerd::ProcessLock lock(file_path);
  if (!lock.Acquire()) {
    LOG(INFO) << "Other hammerd process is running, exit.";
    return EXIT_SUCCESS;
  }

  std::string image;
  if (!base::ReadFileToString(base::FilePath(FLAGS_image_path), &image)) {
    LOG(ERROR) << "Image file is not found: " << FLAGS_image_path;
    return EXIT_FAILURE;
  }

  if (FLAGS_vendor_id < 0 || FLAGS_product_id < 0) {
    LOG(ERROR) << "Must specify USB vendor ID and product ID.";
    return EXIT_FAILURE;
  }

  // The message loop registers a task runner with the current thread, which
  // is used by DBusWrapper to send signals.
  base::MessageLoop message_loop;
  hammerd::HammerUpdater updater(
      image, FLAGS_vendor_id, FLAGS_product_id, FLAGS_usb_bus, FLAGS_usb_port);
  bool ret = updater.Run();
  return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
