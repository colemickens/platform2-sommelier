// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/system_mounter.h"

#include <errno.h>
#include <sys/mount.h>

#include <string>
#include <utility>

#include <base/logging.h>

#include "cros-disks/mount_options.h"
#include "cros-disks/mount_point.h"
#include "cros-disks/platform.h"

namespace cros_disks {

namespace {

MountErrorType UnmountImpl(const Platform* platform,
                           const base::FilePath& path,
                           bool lazy) {
  CHECK(!path.empty());
  const int flags = lazy ? MNT_DETACH : 0;
  return platform->Unmount(path.value(), flags);
}

}  // namespace

SystemUnmounter::SystemUnmounter(const Platform* platform,
                                 UnmountType unmount_type)
    : platform_(platform), unmount_type_(unmount_type) {}

SystemUnmounter::~SystemUnmounter() = default;

MountErrorType SystemUnmounter::Unmount(const MountPoint& mountpoint) {
  MountErrorType error = UnmountImpl(platform_, mountpoint.path(),
                                     unmount_type_ == UnmountType::kLazy);
  if (error == MountErrorType::MOUNT_ERROR_PATH_ALREADY_MOUNTED &&
      unmount_type_ == UnmountType::kLazyFallback) {
    LOG(WARNING) << "Device is busy, trying lazy unmount on "
                 << mountpoint.path().value();
    error = UnmountImpl(platform_, mountpoint.path(), true);
  }
  return error;
}

SystemMounter::SystemMounter(const std::string& filesystem_type,
                             const Platform* platform)
    : Mounter(filesystem_type), platform_(platform) {}

SystemMounter::~SystemMounter() = default;

std::unique_ptr<MountPoint> SystemMounter::Mount(
    const std::string& source,
    const base::FilePath& target_path,
    std::vector<std::string> options,
    MountErrorType* error) const {
  MountOptions mount_options;
  // If the |options| vector contains uid/gid options, these need to be accepted
  // by MountOptions::Initialize(). To do this, |set_user_and_group_id| must be
  // true. However, if |options| doesn't have these options,
  // MountOptions::Initialize() won't create these, because the
  // |default_user_id| and |default_group_id arguments| are empty.
  mount_options.Initialize(options, true /* set_user_and_group_id */, "", "");
  auto flags_and_data = mount_options.ToMountFlagsAndData();

  *error = platform_->Mount(source, target_path.value(), filesystem_type(),
                            flags_and_data.first, flags_and_data.second);
  if (*error != MountErrorType::MOUNT_ERROR_NONE) {
    return nullptr;
  }

  return std::make_unique<MountPoint>(
      target_path, std::make_unique<SystemUnmounter>(
                       platform_, SystemUnmounter::UnmountType::kLazyFallback));
}

bool SystemMounter::CanMount(const std::string& source,
                             const std::vector<std::string>& options,
                             base::FilePath* suggested_dir_name) const {
  if (source.empty()) {
    *suggested_dir_name = base::FilePath("disk");
  } else {
    *suggested_dir_name = base::FilePath(source).BaseName();
  }
  return true;
}

}  // namespace cros_disks
