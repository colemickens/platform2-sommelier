// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/time/time.h>
#include <brillo/daemons/daemon.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <base/strings/stringprintf.h>

#include "biod/cros_fp_device.h"
#include "biod/cros_fp_updater.h"

namespace {

constexpr char kHelpText[] =
    "bio_fw_updater ensures the fingerprint mcu has the latest firmware\n";

void LogFWFileVersion(const biod::CrosFpFirmware& fw) {
  biod::CrosFpFirmware::ImageVersion ver = fw.GetVersion();
  LOG(INFO) << "FWFile RO Version: '" << ver.ro_version << "'";
  LOG(INFO) << "FWFile RW Version: '" << ver.rw_version << "'";
}

void LogFPMCUVersion(const biod::CrosFpDevice::EcVersion& ver) {
  LOG(INFO) << "FPMCU RO Version: '" << ver.ro_version << "'";
  LOG(INFO) << "FPMCU RW Version: '" << ver.rw_version << "'";
  LOG(INFO) << "FPMCU Active Image: "
            << biod::CrosFpDeviceUpdate::EcCurrentImageToString(
                   ver.current_image);
}

void LogSetupDirectory(const base::FilePath& log_dir_path) {
  const auto log_file_path = log_dir_path.Append(base::StringPrintf(
      "bio_fw_updater.%s",
      brillo::GetTimeAsLogString(base::Time::Now()).c_str()));

  brillo::UpdateLogSymlinks(log_dir_path.Append("bio_fw_updater.LATEST"),
                            log_dir_path.Append("bio_fw_updater.PREVIOUS"),
                            log_file_path);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_FILE;
  logging_settings.log_file = log_file_path.value().c_str();
  logging_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  logging::InitLogging(logging_settings);
  logging::SetLogItems(true,    // process ID
                       true,    // thread ID
                       true,    // timestamp
                       false);  // tickcount
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(log_dir, "/var/log/biod", "Directory where logs are written.");
  brillo::FlagHelper::Init(argc, argv, kHelpText);

  const auto log_dir_path = base::FilePath(FLAGS_log_dir);
  if (base::DirectoryExists(log_dir_path)) {
    LogSetupDirectory(log_dir_path);
  } else {
    LOG(ERROR) << "Log directory '" << log_dir_path.value()
               << "' does not exist, using syslog and stderr logging.";
    brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty |
                    brillo::kLogHeader);
  }

  // Check for firmware disable mechanism
  if (biod::updater::UpdateDisallowed()) {
    LOG(INFO) << "FPMCU update disabled, exiting.";
    return EXIT_SUCCESS;
  }

  // Find a firmware file that matches the firmware file pattern
  base::FilePath file;
  auto status = biod::updater::FindFirmwareFile(
      base::FilePath(biod::updater::kFirmwareDir), &file);

  if (status == biod::updater::FindFirmwareFileStatus::kNoDirectory ||
      status == biod::updater::FindFirmwareFileStatus::kFileNotFound) {
    LOG(INFO) << "No firmware "
              << ((status ==
                   biod::updater::FindFirmwareFileStatus::kNoDirectory)
                      ? "directory"
                      : "file")
              << " on rootfs, exiting.";

    return EXIT_SUCCESS;
  }
  if (status == biod::updater::FindFirmwareFileStatus::kMultipleFiles) {
    LOG(ERROR) << "Found more than one firmware file, aborting.";
    return EXIT_FAILURE;
  }

  biod::CrosFpFirmware fw(file);
  if (!fw.IsValid()) {
    LOG(ERROR) << "Failed to load firmware file '" << fw.GetPath().value()
               << "': " << fw.GetStatusString();
    LOG(ERROR) << "We are aborting update.";
    return EXIT_FAILURE;
  }
  LogFWFileVersion(fw);

  biod::CrosFpDeviceUpdate ec_device;
  biod::CrosFpBootUpdateCtrl boot_ctrl;

  biod::CrosFpDevice::EcVersion ecver;
  if (!ec_device.GetVersion(&ecver)) {
    LOG(INFO) << "Failed to fetch EC version, aborting.";
    return EXIT_FAILURE;
  }
  LogFPMCUVersion(ecver);

  switch (biod::updater::DoUpdate(ec_device, boot_ctrl, fw)) {
    case biod::updater::UpdateStatus::kUpdateFailed:
      LOG(ERROR) << "Failed to update FPMCU firmware, aborting.";
      return EXIT_FAILURE;
    case biod::updater::UpdateStatus::kUpdateSucceeded:
      if (!ec_device.GetVersion(&ecver)) {
        LOG(ERROR) << "Failed to fetch final EC version, update failed.";
        return EXIT_FAILURE;
      }
      LogFPMCUVersion(ecver);
      LOG(INFO) << "The update was successful.";
      break;
    case biod::updater::UpdateStatus::kUpdateNotNecessary:
      LOG(INFO) << "Update was not necessary.";
      break;
  }

  return EXIT_SUCCESS;
}
