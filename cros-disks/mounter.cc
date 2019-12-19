// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mounter.h"

#include <sys/mount.h>

#include <utility>

#include <base/logging.h>

#include "cros-disks/error_logger.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/mount_point.h"
#include "cros-disks/platform.h"
#include "cros-disks/quote.h"

namespace cros_disks {

MounterCompat::MounterCompat(std::unique_ptr<Mounter> mounter,
                             const std::string& source,
                             const base::FilePath& target_path,
                             const MountOptions& mount_options)
    : Mounter(mounter->filesystem_type()),
      mounter_(std::move(mounter)),
      source_(source),
      target_path_(target_path),
      mount_options_(mount_options) {}

MounterCompat::MounterCompat(const std::string& filesystem_type,
                             const std::string& source,
                             const base::FilePath& target_path,
                             const MountOptions& mount_options)
    : Mounter(filesystem_type),
      source_(source),
      target_path_(target_path),
      mount_options_(mount_options) {}

MounterCompat::~MounterCompat() = default;

MountErrorType MounterCompat::MountOld() {
  MountErrorType error = MOUNT_ERROR_NONE;
  std::unique_ptr<MountPoint> mount_point =
      Mount(source_, target_path_, mount_options().options(), &error);
  if (mount_point) {
    LOG(INFO) << "Mounted " << quote(source_) << " to " << quote(target_path_)
              << " as filesystem " << quote(filesystem_type())
              << " with options " << quote(mount_options_.ToString());
    // Release ownership of the filesystem so that it isn't unmounted when
    // |mount_point| goes out of scope. MountOld() is used in places that have
    // not been converted to track MountPoint objects. In these cases, ownership
    // of the mount point is implicitly transferred to the caller, which is
    // required to unmount when necessary. In most cases, ownership will bubble
    // down to MountManager, which will then create a new MountPoint object
    // holding the mount point.
    mount_point->Release();
  } else {
    LOG(ERROR) << "Cannot mount " << quote(source_) << " to "
               << quote(target_path_) << " as filesystem "
               << quote(filesystem_type()) << " with options "
               << quote(mount_options_.ToString()) << ": " << error;
  }
  return error;
}

std::unique_ptr<MountPoint> MounterCompat::Mount(
    const std::string& source,
    const base::FilePath& target_path,
    std::vector<std::string> options,
    MountErrorType* error) const {
  CHECK(mounter_) << "Method must be overridden if mounter is not set";
  return mounter_->Mount(source, target_path, options, error);
}

bool MounterCompat::CanMount(const std::string& source,
                             const std::vector<std::string>& options,
                             base::FilePath* suggested_dir_name) const {
  *suggested_dir_name = base::FilePath("dir");
  return true;
}

}  // namespace cros_disks
