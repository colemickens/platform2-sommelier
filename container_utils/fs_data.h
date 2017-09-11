// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTAINER_UTILS_FS_DATA_H_
#define CONTAINER_UTILS_FS_DATA_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/macros.h>

#include "container_utils/device_jail_control.h"

struct stat;

namespace device_jail {

class FsData {
 public:
  // Initialize FsData for a device_jail_fs with the backing directory
  // given by |dev_dir| and the mount point given by |mount_point|.
  static std::unique_ptr<FsData> Create(const std::string& dev_dir,
                                        const std::string& mount_point);

  virtual ~FsData();

  // Get the stat for a jailed version of the device given by |path|.
  // If one does not exist already, create it and try to ensure that it
  // spawned correctly.
  // Returns -1 and sets errno on error, or 0 on success.
  int GetStatForJail(const std::string& path, struct stat* file_stat);

  int root_fd() const { return root_fd_.get(); }

  const std::string& mount_point() const { return mount_point_; }

 private:
  FsData(base::ScopedFD root_fd,
         std::string dev_dir,
         std::string mount_point,
         std::unique_ptr<DeviceJailControl> jail_control)
    : root_fd_(std::move(root_fd)),
      dev_dir_(std::move(dev_dir)),
      mount_point_(std::move(mount_point)),
      jail_control_(std::move(jail_control)) {}

  // FD representing the directory this filesystem is backed by.
  base::ScopedFD root_fd_;

  std::string dev_dir_;
  std::string mount_point_;

  // Proxy object for the device jail control device.
  std::unique_ptr<DeviceJailControl> jail_control_;
  // Jails this FS should destroy when it is unmounted.
  std::vector<std::string> owned_devices_;

  DISALLOW_COPY_AND_ASSIGN(FsData);
};

}  // namespace device_jail

#endif  // CONTAINER_UTILS_FS_DATA_H_
