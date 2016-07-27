// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "loop_mounter.h"

#include <string>

#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <base/strings/stringprintf.h>

namespace imageloader {

namespace {

const int MAX_RETRY = 5;
enum class MountStatus { FAIL, RETRY, SUCCESS };

MountStatus MountLoopDevice(const base::ScopedFD& image_fd,
                            const base::FilePath& mount_point) {
  base::ScopedFD loopctl_fd(open("/dev/loop-control", O_RDONLY | O_CLOEXEC));
  if (!loopctl_fd.is_valid()) {
    PLOG(ERROR) << "loopctl_fd";
    return MountStatus::FAIL;
  }
  int device_free_number = ioctl(loopctl_fd.get(), LOOP_CTL_GET_FREE);
  if (device_free_number < 0) {
    PLOG(ERROR) << "ioctl : LOOP_CTL_GET_FREE";
    return MountStatus::FAIL;
  }
  std::string device_path =
      base::StringPrintf("/dev/loop%d", device_free_number);
  base::ScopedFD loop_device_fd(
      open(device_path.c_str(), O_RDONLY | O_CLOEXEC));
  if (!loop_device_fd.is_valid()) {
    PLOG(ERROR) << "loop_device_fd";
    LOG(ERROR) << "mount_point: " << mount_point.value();
    return MountStatus::FAIL;
  }

  if (ioctl(loop_device_fd.get(), LOOP_SET_FD, image_fd.get()) < 0) {
    if (errno == EBUSY) {
      // We need to retry because another program could grap the loop device,
      // resulting in an EBUSY error. If that happens, run again and grab a new
      // device.
      return MountStatus::RETRY;
    }
    PLOG(ERROR) << "ioctl: LOOP_SET_FD";
    LOG(ERROR) << "mount_point: " << mount_point.value();
    return MountStatus::FAIL;
  }
  if (mount(device_path.c_str(), mount_point.value().c_str(), "squashfs",
            MS_RDONLY | MS_NOSUID | MS_NODEV, "") < 0) {
    PLOG(ERROR) << "mount";
    LOG(ERROR) << "mount_point: " << mount_point.value();
    ioctl(loop_device_fd.get(), LOOP_CLR_FD, 0);
    return MountStatus::FAIL;
  }
  return MountStatus::SUCCESS;
}

} // namespace

bool LoopMounter::Mount(const base::ScopedFD& image_fd,
                        const base::FilePath& mount_point) {
  for (int i = 0; i < MAX_RETRY; i++) {
    MountStatus status = MountLoopDevice(image_fd, mount_point);
    switch (status) {
      case MountStatus::FAIL:
        return false;
      case MountStatus::RETRY:
        continue;
      case MountStatus::SUCCESS:
        return true;
    }
  }
  return false;
}

}  // namespace imageloader
