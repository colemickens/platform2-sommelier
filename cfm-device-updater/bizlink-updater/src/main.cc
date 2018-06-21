// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-updater/bizlink-updater/src/dp_aux_ctrl.h"
#include "cfm-device-updater/bizlink-updater/src/mcdp_chip_ctrl.h"
#include "cfm-device-updater/bizlink-updater/src/puma_fw_ctrl.h"

#include <base/logging.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <brillo/flag_helper.h>

#include <fcntl.h>

using base::FilePath;

namespace {

const FilePath::StringType kDpDevPath = FILE_PATH_LITERAL("/dev/drm_dp_aux");

}

int main(int argc, char* argv[]) {
  // Flag for run FW update. If false, print FW bin and device version.
  DEFINE_bool(update, false, "Run FW update.");
  DEFINE_string(fw_path, "/lib/firmware/bizlink/megachips-firmware.bin",
                "Absolute FW path to flash.");
  DEFINE_bool(force, false, "Skip FW version check and force update.");
  brillo::FlagHelper::Init(argc, argv, "bizlink-updater");

  // Configure logging to syslog.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  // Read FW bin file info.
  const FilePath fw_path(FLAGS_fw_path);
  uint32_t fw_bin_version;
  if (!bizlink_updater::GetFwBinInfo(fw_path, &fw_bin_version)) {
    LOG(ERROR) << "Failed to get FW file version.";
    return 1;
  }

  // Find valid dp aux port for the dongle.
  bizlink_updater::McdpChipInfo chip_info;
  int valid_drm_port;
  if (!bizlink_updater::GetValidDevice(&chip_info, &valid_drm_port)) {
    return 1;
  }

  // Flash new firmware to the chip.
  if (FLAGS_update) {
    if (!FLAGS_force && (fw_bin_version == chip_info.fw_version)) {
      LOG(INFO) << "Same FW version, no update required.";
      return 0;
    }
    LOG(INFO) << "Use valid DP AUX port " << valid_drm_port;
    FilePath dev_path(kDpDevPath + std::to_string(valid_drm_port));
    base::ScopedFD dev_fd(open(dev_path.value().c_str(), O_RDWR | O_CLOEXEC));
    if (!dev_fd.is_valid()) {
      LOG(ERROR) << "Failed to open DP AUX port.";
      return 1;
    }
    if (!bizlink_updater::FlashNewFw(fw_path, dev_fd, chip_info)) {
      return 1;
    }
  }

  return 0;
}
