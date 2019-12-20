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

// A MountPoint that uses the umount() syscall for unmounting.
class SystemMountPoint : public MountPoint {
 public:
  SystemMountPoint(const base::FilePath& path, const Platform* platform)
      : MountPoint(path), platform_(platform) {}

  ~SystemMountPoint() override { DestructorUnmount(); }

 protected:
  MountErrorType UnmountImpl() override {
    MountErrorType error = platform_->Unmount(path().value(), 0);
    if (error == MOUNT_ERROR_PATH_ALREADY_MOUNTED) {
      LOG(WARNING) << "Device is busy, trying lazy unmount on "
                   << path().value();
      error = platform_->Unmount(path().value(), MNT_DETACH);
    }
    return error;
  }

 private:
  const Platform* platform_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SystemMountPoint);
};

}  // namespace

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
  if (*error != MOUNT_ERROR_NONE) {
    return nullptr;
  }

  return std::make_unique<SystemMountPoint>(target_path, platform_);
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
