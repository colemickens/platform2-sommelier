// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_MOUNT_POINT_H_
#define CROS_DISKS_MOUNT_POINT_H_

#include <memory>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <chromeos/dbus/service_constants.h>

namespace cros_disks {

class Unmounter;

// Class representing a mount created by a mounter.
class MountPoint {
 public:
  MountPoint(const base::FilePath& path, std::unique_ptr<Unmounter> unmounter);

  // The destructor calls unmounter to unmount the mount point.
  ~MountPoint();

  // Releases (leaks) the ownership of the mount point.
  // Until all places handle ownership of mount points properly
  // it's necessary to be able to leave the mount alone.
  void Release();

  // Unmounts right now using the unmounter.
  MountErrorType Unmount();

  const base::FilePath& path() const { return path_; }

 private:
  const base::FilePath path_;
  std::unique_ptr<Unmounter> unmounter_;

  DISALLOW_COPY_AND_ASSIGN(MountPoint);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_MOUNT_POINT_H_
