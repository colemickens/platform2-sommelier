// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SYSTEM_MOUNTER_H_
#define CROS_DISKS_SYSTEM_MOUNTER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "cros-disks/mount_point.h"
#include "cros-disks/mounter.h"

namespace cros_disks {

class Platform;

// A class that uses umount() syscall for unmounting.
class SystemUnmounter : public Unmounter {
 public:
  enum class UnmountType { kNormal, kLazy, kLazyFallback };
  SystemUnmounter(const Platform* platform, UnmountType unmount_type);
  ~SystemUnmounter() override;

  MountErrorType Unmount(const MountPoint& mountpoint) override;

 private:
  const Platform* const platform_;
  const UnmountType unmount_type_;

  DISALLOW_COPY_AND_ASSIGN(SystemUnmounter);
};

// A class for mounting a device file using the system mount() call.
class SystemMounter : public Mounter {
 public:
  SystemMounter(const std::string& filesystem_type, const Platform* platform);
  ~SystemMounter() override;

  // Mounts a device file using the system mount() call.
  std::unique_ptr<MountPoint> Mount(const std::string& source,
                                    const base::FilePath& target_path,
                                    std::vector<std::string> options,
                                    MountErrorType* error) const override;

  // As there is no way to figure out beforehand if that would work, always
  // returns true, so this mounter is a "catch-all".
  bool CanMount(const std::string& source,
                const std::vector<std::string>& options,
                base::FilePath* suggested_dir_name) const override;

 private:
  const Platform* const platform_;

  DISALLOW_COPY_AND_ASSIGN(SystemMounter);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SYSTEM_MOUNTER_H_
