// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/format_manager.h"

#include <string>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/process.h>

#include "cros-disks/format_manager_observer_interface.h"

namespace {

// Expected locations of an external format program
const char* const kFormatProgramPaths[] = {
    "/usr/sbin/mkfs.",
    "/bin/mkfs.",
    "/sbin/mkfs.",
    "/usr/bin/mkfs.",
};

// Supported file systems
const char* const kSupportedFilesystems[] = {
    "vfat",
    "exfat",
    "ntfs",
};

const char kDefaultLabel[] = "UNTITLED";

}  // namespace

namespace cros_disks {

FormatManager::FormatManager(brillo::ProcessReaper* process_reaper)
    : process_reaper_(process_reaper), weak_ptr_factory_(this) {}

FormatManager::~FormatManager() {}

FormatErrorType FormatManager::StartFormatting(const std::string& device_path,
                                               const std::string& device_file,
                                               const std::string& filesystem) {
  // Check if the file system is supported for formatting
  if (!IsFilesystemSupported(filesystem)) {
    LOG(WARNING) << filesystem << " filesystem is not supported for formatting";
    return FORMAT_ERROR_UNSUPPORTED_FILESYSTEM;
  }

  // Localize mkfs on disk
  std::string format_program = GetFormatProgramPath(filesystem);
  if (format_program.empty()) {
    LOG(WARNING) << "Could not find a format program for filesystem '"
                 << filesystem << "'";
    return FORMAT_ERROR_FORMAT_PROGRAM_NOT_FOUND;
  }

  if (base::ContainsKey(format_process_, device_path)) {
    LOG(WARNING) << "Device '" << device_path << "' is already being formatted";
    return FORMAT_ERROR_DEVICE_BEING_FORMATTED;
  }

  SandboxedProcess* process = &format_process_[device_path];
  process->SetNoNewPrivileges();
  process->NewMountNamespace();
  process->NewIpcNamespace();
  process->NewNetworkNamespace();
  process->SetCapabilities(0);
  if (!process->EnterPivotRoot()) {
    LOG(WARNING) << "Could not enter pivot root";
    return FORMAT_ERROR_FORMAT_PROGRAM_FAILED;
  }
  if (!process->SetUpMinimalMounts()) {
    LOG(WARNING) << "Could not set up minimal mounts for jail";
    return FORMAT_ERROR_FORMAT_PROGRAM_FAILED;
  }

  // Open device_file so we can pass only the fd path to the format program.
  base::File dev_file(base::FilePath(device_file), base::File::FLAG_OPEN |
                                                       base::File::FLAG_READ |
                                                       base::File::FLAG_WRITE);
  if (!dev_file.IsValid()) {
    LOG(WARNING) << "Could not open " << device_file << " for formatting";
    return FORMAT_ERROR_FORMAT_PROGRAM_FAILED;
  }
  if (!process->PreserveFile(dev_file)) {
    LOG(WARNING) << "Could not preserve device fd";
    return FORMAT_ERROR_FORMAT_PROGRAM_FAILED;
  }
  process->CloseOpenFds();

  process->AddArgument(format_program);

  // Allow to create filesystem across the entire device.
  if (filesystem == "vfat") {
    process->AddArgument("-I");
    // FAT type should be predefined, because mkfs autodetection is faulty.
    process->AddArgument("-F");
    process->AddArgument("32");
    process->AddArgument("-n");
    process->AddArgument(kDefaultLabel);
  } else if (filesystem == "exfat") {
    process->AddArgument("-n");
    process->AddArgument(kDefaultLabel);
  } else if (filesystem == "ntfs") {
    process->AddArgument("--quick");
    process->AddArgument("--label");
    process->AddArgument(kDefaultLabel);
  }

  process->AddArgument(
      base::StringPrintf("/dev/fd/%d", dev_file.GetPlatformFile()));
  if (!process->Start()) {
    LOG(WARNING) << "Cannot start a process for formatting '" << device_path
                 << "' as filesystem '" << filesystem << "'";
    format_process_.erase(device_path);
    return FORMAT_ERROR_FORMAT_PROGRAM_FAILED;
  }

  process_reaper_->WatchForChild(
      FROM_HERE, process->pid(),
      base::Bind(&FormatManager::OnFormatProcessTerminated,
                 weak_ptr_factory_.GetWeakPtr(), device_path));
  return FORMAT_ERROR_NONE;
}

void FormatManager::OnFormatProcessTerminated(const std::string& device_path,
                                              const siginfo_t& info) {
  format_process_.erase(device_path);
  FormatErrorType error_type = FORMAT_ERROR_UNKNOWN;
  switch (info.si_code) {
    case CLD_EXITED:
      if (info.si_status == 0) {
        error_type = FORMAT_ERROR_NONE;
        LOG(INFO) << "Process " << info.si_pid << " for formatting '"
                  << device_path << "' completed successfully";
      } else {
        error_type = FORMAT_ERROR_FORMAT_PROGRAM_FAILED;
        LOG(ERROR) << "Process " << info.si_pid << " for formatting '"
                   << device_path << "' exited with a status "
                   << info.si_status;
      }
      break;

    case CLD_DUMPED:
    case CLD_KILLED:
      error_type = FORMAT_ERROR_FORMAT_PROGRAM_FAILED;
      LOG(ERROR) << "Process " << info.si_pid << " for formatting '"
                 << device_path << "' killed by a signal " << info.si_status;
      break;

    default:
      break;
  }
  if (observer_)
    observer_->OnFormatCompleted(device_path, error_type);
}

std::string FormatManager::GetFormatProgramPath(
    const std::string& filesystem) const {
  for (const char* program_path : kFormatProgramPaths) {
    std::string path = program_path + filesystem;
    if (base::PathExists(base::FilePath(path)))
      return path;
  }
  return std::string();
}

bool FormatManager::IsFilesystemSupported(const std::string& filesystem) const {
  for (const char* supported_filesystem : kSupportedFilesystems) {
    if (filesystem == supported_filesystem)
      return true;
  }
  return false;
}

}  // namespace cros_disks
