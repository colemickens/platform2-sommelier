// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "container_utils/device_jail_control.h"

#include <fcntl.h>
#include <linux/device_jail.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/logging.h>

namespace {

const char kJailControlPath[] = "/dev/jail-control";

struct UdevDeviceUnref {
  inline void operator()(struct udev_device* ptr) {
    if (ptr)
      udev_device_unref(ptr);
  }
};
using UdevDevice = std::unique_ptr<struct udev_device, UdevDeviceUnref>;

}  // namespace

namespace device_jail {

// static
std::unique_ptr<DeviceJailControl> DeviceJailControl::Create() {
  base::ScopedFD control_fd(open(kJailControlPath, O_RDWR));
  if (!control_fd.is_valid()) {
    PLOG(ERROR) << "unable to open control device";
    return std::unique_ptr<DeviceJailControl>();
  }

  struct udev* udev = udev_new();
  if (!udev) {
    LOG(ERROR) << "unable to get udev context";
    return std::unique_ptr<DeviceJailControl>();
  }

  return std::unique_ptr<DeviceJailControl>(
      new DeviceJailControl(std::move(control_fd), udev));
}

DeviceJailControl::~DeviceJailControl() {
  if (udev_)
    udev_unref(udev_);
}

DeviceJailControl::AddResult DeviceJailControl::AddDevice(
    const std::string& path, std::string* jail_path) {
  if (!jail_path)
    return AddResult::ERROR;

  struct jail_control_add_dev arg;
  arg.path = path.c_str();

  bool exists = false;
  int ret = ioctl(control_fd_.get(), JAIL_CONTROL_ADD_DEVICE, &arg);
  if (ret < 0) {
    if (errno == EEXIST) {
      exists = true;
    } else {
      PLOG(INFO) << "failed to create device";
      return AddResult::ERROR;
    }
  }

  // The udev library doesn't implement its own locking, so we have
  // to make sure nobody else is using the struct udev.
  base::AutoLock _l(udev_lock_);
  UdevDevice device(udev_device_new_from_devnum(udev_, 'c', arg.devnum));
  if (!device) {
    PLOG(ERROR) << "udev doesn't recognize the jail device";
    return AddResult::ERROR;
  }

  // Wait a few ms for udev to run rules on the device. Shouldn't take
  // longer than 3ms, but poll for a little longer to be sure.
  // TODO(ejcaruso): replace this with an async call that uses udev_monitor
  // to determine when the device is initialized.
  for (int i = 0; i < 10; i++) {
    if (udev_device_get_is_initialized(device.get()))
      break;
    usleep(1000);
  }

  *jail_path = udev_device_get_devnode(device.get());
  if (!udev_device_get_is_initialized(device.get()))
    LOG(WARNING) << "udev is taking a while to initialize " << *jail_path;
  return exists ? AddResult::ALREADY_EXISTS : AddResult::CREATED;
}

bool DeviceJailControl::RemoveDevice(const std::string& path) {
  struct stat buf;

  int ret = stat(path.c_str(), &buf);
  if (ret < 0) {
    PLOG(ERROR) << "could not stat " << path;
    return false;
  }

  if (!S_ISCHR(buf.st_mode)) {
    LOG(ERROR) << path << " is not a character device";
    return false;
  }

  ret = ioctl(control_fd_.get(), JAIL_CONTROL_REMOVE_DEVICE, &buf.st_rdev);
  if (ret < 0) {
    PLOG(ERROR) << "error removing device";
    return false;
  }

  return true;
}

}  // namespace device_jail
