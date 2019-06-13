// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "container_utils/fs_data.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include <base/compiler_specific.h>
#include <base/logging.h>
#include <base/memory/free_deleter.h>

namespace device_jail {

FsData::~FsData() {
  for (const std::string& jail_path : owned_devices_) {
    jail_control_->RemoveDevice(jail_path);
  }
}

// static
std::unique_ptr<FsData> FsData::Create(const std::string& dev_dir,
                                       const std::string& mount_point) {
  base::ScopedFD root_fd(
      open(dev_dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (!root_fd.is_valid()) {
    PLOG(ERROR) << "couldn't open root directory";
    return std::unique_ptr<FsData>();
  }

  std::unique_ptr<DeviceJailControl> jail_control =
      DeviceJailControl::Create();
  if (!jail_control)
    return std::unique_ptr<FsData>();

  std::unique_ptr<char, base::FreeDeleter> real_mount_point(
      realpath(mount_point.c_str(), nullptr));
  if (!real_mount_point) {
    PLOG(ERROR) << "couldn't resolve mount point";
    return std::unique_ptr<FsData>();
  }

  return std::unique_ptr<FsData>(new FsData(std::move(root_fd),
                                            dev_dir,
                                            real_mount_point.get(),
                                            std::move(jail_control)));
}

int FsData::GetStatForJail(const std::string& path, struct stat* file_stat) {
  std::string jail_path;
  switch (jail_control_->AddDevice(path, &jail_path)) {
  case DeviceJailControl::AddResult::ERROR:
    return -ENOENT;
  case DeviceJailControl::AddResult::CREATED:
    owned_devices_.push_back(jail_path);
    FALLTHROUGH;
  case DeviceJailControl::AddResult::ALREADY_EXISTS:
    return stat(jail_path.c_str(), file_stat);
  }

  LOG(FATAL) << "Unhandled AddDevice return";
  return -ENOENT;
}

}  // namespace device_jail
