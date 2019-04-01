// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mounter.h"

#include <sys/mount.h>

#include <utility>

#include <base/logging.h>

#include "cros-disks/mount_options.h"
#include "cros-disks/mount_point.h"
#include "cros-disks/platform.h"

namespace cros_disks {

MounterCompat::MounterCompat(const std::string& filesystem_type,
                             const std::string& source,
                             const base::FilePath& target_path,
                             const MountOptions& mount_options)
    : Mounter(filesystem_type),
      source_(source),
      target_path_(target_path),
      mount_options_(mount_options) {}

MounterCompat::~MounterCompat() {}

MountErrorType MounterCompat::Mount() {
  MountErrorType error = MOUNT_ERROR_NONE;
  mountpoint_ = Mount({}, {}, {}, &error);
  if (mountpoint_) {
    LOG(INFO) << "Mounted '" << source_ << "' to '" << target_path_.value()
              << "' as filesystem '" << filesystem_type() << "' with options '"
              << mount_options_.ToString() << "'";
  } else {
    LOG(ERROR) << "Failed to mount '" << source_ << "' to '"
               << target_path_.value() << "' as filesystem '"
               << filesystem_type() << "' with options '"
               << mount_options_.ToString() << "'";
  }
  return error;
}

std::unique_ptr<MountPoint> MounterCompat::Mount(
    const std::string& source,
    const base::FilePath& target_path,
    std::vector<std::string> options,
    MountErrorType* error) const {
  *error = MountImpl();
  if (*error == MOUNT_ERROR_NONE) {
    // Makes mountpoint that won't unmount for compatibility with old behavior.
    return std::make_unique<MountPoint>(target_path_, nullptr);
  }
  return nullptr;
}

bool MounterCompat::CanMount(const std::string& source,
                             const std::vector<std::string>& options,
                             base::FilePath* suggested_dir_name) const {
  *suggested_dir_name = base::FilePath("dir");
  return true;
}

}  // namespace cros_disks
