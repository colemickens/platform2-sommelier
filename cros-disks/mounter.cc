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

MountErrorType MounterCompat::Mount() {
  MountErrorType error = MOUNT_ERROR_NONE;
  mountpoint_ = Mount({}, {}, {}, &error);
  if (mountpoint_) {
    LOG(INFO) << "Mounted " << quote(source_) << " to " << quote(target_path_)
              << " as filesystem " << quote(filesystem_type())
              << " with options " << quote(mount_options_.ToString());
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
  *error = MountImpl();
  if (*error != MOUNT_ERROR_NONE) {
    return nullptr;
  }

  // Makes mountpoint that won't unmount for compatibility with old behavior.
  return std::make_unique<MountPoint>(target_path_, nullptr);
}

bool MounterCompat::CanMount(const std::string& source,
                             const std::vector<std::string>& options,
                             base::FilePath* suggested_dir_name) const {
  *suggested_dir_name = base::FilePath("dir");
  return true;
}

MountErrorType MounterCompat::MountImpl() const {
  MountErrorType error = MOUNT_ERROR_NONE;
  // This default implementation implies using a new passed mounter to perform
  // the mounting. For legacy signature mounter is null, but this method is
  // overriden and this code is never called.
  CHECK(mounter_) << "Method must be overriden if mounter is not set";
  auto mountpoint = mounter_->Mount(source(), target_path(),
                                    mount_options().options(), &error);
  if (mountpoint) {
    // Leak the mountpoint.
    mountpoint->Release();
  }
  return error;
}

}  // namespace cros_disks
