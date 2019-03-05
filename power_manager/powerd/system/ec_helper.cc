// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ec_helper.h"

#include <string>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

const base::FilePath k318WakeAngleSysPath(
    "/sys/class/chromeos/cros_ec/kb_wake_angle");
const base::FilePath k314IioLinkPath("/dev/cros-ec-accel/0");
const base::FilePath k314IioSysfsPath("/sys/bus/iio/devices");
const base::FilePath k314AccelNodeName("in_angl_offset");

}  // namespace

EcHelper::EcHelper() : wake_angle_supported_(false), cached_wake_angle_(-1) {
  if (base::PathExists(k318WakeAngleSysPath)) {  // Kernel 3.18 and later
    wake_angle_sysfs_node_ = k318WakeAngleSysPath;
    wake_angle_supported_ = true;
    VLOG(1) << "Accessing EC wake angle through 3.18+ sysfs node: "
            << wake_angle_sysfs_node_.value();
  } else if (base::IsLink(k314IioLinkPath)) {  // Kernel 3.14
    base::FilePath iio_dev_path;
    if (!base::ReadSymbolicLink(k314IioLinkPath, &iio_dev_path)) {
      LOG(ERROR) << "Cannot read link target of " << k314IioLinkPath.value();
      return;
    }
    iio_dev_path = iio_dev_path.BaseName();
    wake_angle_sysfs_node_ =
        k314IioSysfsPath.Append(iio_dev_path).Append(k314AccelNodeName);
    if (!base::PathExists(wake_angle_sysfs_node_)) {
      LOG(ERROR) << "Cannot find EC wake angle node: "
                 << wake_angle_sysfs_node_.value();
      return;
    }
    wake_angle_supported_ = true;
    VLOG(1) << "Accessing EC wake angle through 3.14 sysfs node: "
            << wake_angle_sysfs_node_.value();
  } else {
    VLOG(1) << "This device does not support EC wake angle control.";
  }
}

EcHelper::~EcHelper() {}

bool EcHelper::IsWakeAngleSupported() {
  return wake_angle_supported_;
}

bool EcHelper::AllowWakeupAsTablet(bool enabled) {
  int new_wake_angle = enabled ? 360 : 180;
  std::string str = base::IntToString(new_wake_angle);
  if (new_wake_angle == cached_wake_angle_) {
    VLOG(1) << "EC wake angle is already set to " << str;
    return true;
  }
  if (base::WriteFile(wake_angle_sysfs_node_, str.c_str(), str.size()) < 0) {
    PLOG(ERROR) << "Failed to set EC wake angle to " << str;
    return false;
  }
  LOG(INFO) << "EC wake angle set to " << str;
  cached_wake_angle_ = new_wake_angle;
  return true;
}

}  // namespace system
}  // namespace power_manager
