// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/format-manager.h"

#include <glib.h>
#include <map>
#include <string>

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/process.h>

#include "cros-disks/cros-disks-server-impl.h"

namespace {

// Expected locations of an external format program
const char* kFormatProgramPaths[] = {
  "/usr/sbin/mkfs.", "/bin/mkfs.", "/sbin/mkfs.", "/usr/bin/mkfs.",
};

// Supported file systems
const char* kSupportedFilesystems[] = {
  "vfat"
};

void OnFormatProcessExit(pid_t pid, gint status, gpointer data) {
  cros_disks::FormatManager* format_manager =
      reinterpret_cast<cros_disks::FormatManager*>(data);
  format_manager->FormattingFinished(pid, status);
}

}  // annoymous namespace


namespace cros_disks {

FormatManager::FormatManager() {
}

FormatManager::~FormatManager() {
}

bool FormatManager::StartFormatting(const std::string& device_path,
    const std::string& filesystem) {
  // Check if the file system is supported for formatting
  if (!IsFilesystemSupported(filesystem)) {
    LOG(WARNING) << filesystem
      << " filesystem is not supported for formatting";
    return false;
  }

  // Localize mkfs on disk
  std::string format_program = GetFormatProgramPath(filesystem);
  if (format_program.empty()) {
    LOG(WARNING) << "Could not find an external format program "
      "for filesystem" << filesystem;
    return false;
  }

  if (format_process_.find(device_path) != format_process_.end()) {
    LOG(WARNING) << "Device '" << device_path
      << "' is already being formatted";
    return false;
  }

  chromeos::ProcessImpl* process = &format_process_[device_path];
  process->AddArg(format_program);
  process->AddArg(device_path);
  if (!process->Start()) {
    LOG(WARNING) << "Cannot start a process for formatting '"
      << device_path << "' as filesystem '" << filesystem << "'";
    format_process_.erase(device_path);
    return false;
  }
  pid_to_device_path_[process->pid()] = device_path;
  g_child_watch_add(process->pid(),
                    &OnFormatProcessExit,
                    this);
  return true;
}

void FormatManager::FormattingFinished(pid_t pid, int status) {
  std::string device_path = pid_to_device_path_[pid];
  format_process_.erase(device_path);
  pid_to_device_path_.erase(pid);
  if (parent_)
    parent_->SignalFormattingFinished(device_path, status);
}

bool FormatManager::StopFormatting(const std::string& device_path) {
  // TODO(sidor): implement
  // When the cancel button is hit
  return false;
}

void FormatManager::set_parent(CrosDisksServer* parent) {
  parent_ = parent;
}

std::string FormatManager::GetFormatProgramPath(
    const std::string& filesystem) const {
  for (size_t i = 0; i < arraysize(kFormatProgramPaths); ++i) {
    std::string path = kFormatProgramPaths[i] + filesystem;
    if (file_util::PathExists(FilePath(path)))
      return path;
  }
  return std::string();
}

bool FormatManager::IsFilesystemSupported(
    const std::string& filesystem) const {
  for (size_t i = 0; i < arraysize(kSupportedFilesystems); ++i) {
    if (filesystem == kSupportedFilesystems[i])
      return true;
  }
  return false;
}

}  // namespace cros_disks
