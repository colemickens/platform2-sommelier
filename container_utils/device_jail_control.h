// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTAINER_UTILS_DEVICE_JAIL_CONTROL_H_
#define CONTAINER_UTILS_DEVICE_JAIL_CONTROL_H_

#include <memory>
#include <string>
#include <utility>

#include <libudev.h>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/synchronization/lock.h>

namespace device_jail {

class DeviceJailControl {
 public:
  enum class AddResult {
    ERROR,
    ALREADY_EXISTS,
    CREATED
  };

  static std::unique_ptr<DeviceJailControl> Create();
  virtual ~DeviceJailControl();

  AddResult AddDevice(const std::string& path, std::string* jail_path);
  bool RemoveDevice(const std::string& path);

 private:
  DeviceJailControl(base::ScopedFD control_fd, struct udev* udev)
      : control_fd_(std::move(control_fd)), udev_(udev) {}

  base::ScopedFD control_fd_;

  struct udev* udev_;
  base::Lock udev_lock_;

  DISALLOW_COPY_AND_ASSIGN(DeviceJailControl);
};

}  // namespace device_jail

#endif  // CONTAINER_UTILS_DEVICE_JAIL_CONTROL_H_
