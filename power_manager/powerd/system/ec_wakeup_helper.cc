// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ec_wakeup_helper.h"

#include <string>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

const base::FilePath k318SysfsPath("/sys/class/chromeos/cros_ec/kb_wake_angle");
const base::FilePath k314IioLinkPath("/dev/cros-ec-accel/0");
const base::FilePath k314IioSysfsPath("/sys/bus/iio/devices");
const base::FilePath k314AccelNodeName("in_angl_offset");

}  // namespace

EcWakeupHelper::EcWakeupHelper()
    : supported_(false),
      cached_wake_angle_(-1) {
  if (base::PathExists(k318SysfsPath)) {  // Kernel 3.18 and later
    sysfs_node_ = k318SysfsPath;
    supported_ = true;
    VLOG(1) << "Accessing EC wake angle through 3.18+ sysfs node: "
            << sysfs_node_.value();
  } else if (base::IsLink(k314IioLinkPath)) {  // Kernel 3.14
    base::FilePath iioDevPath;
    if (!base::ReadSymbolicLink(k314IioLinkPath, &iioDevPath)) {
      LOG(ERROR) << "Cannot read link target of " << k314IioLinkPath.value();
      return;
    }
    iioDevPath = iioDevPath.BaseName();
    sysfs_node_ = k314IioSysfsPath.Append(iioDevPath).Append(k314AccelNodeName);
    if (!base::PathExists(sysfs_node_)) {
      LOG(ERROR) << "Cannot find EC wake angle node: " << sysfs_node_.value();
      return;
    }
    supported_ = true;
    VLOG(1) << "Accessing EC wake angle through 3.14 sysfs node: "
            << sysfs_node_.value();
  } else {
    VLOG(1) << "This device does not support EC wake angle control.";
  }
}

EcWakeupHelper::~EcWakeupHelper() {}

bool EcWakeupHelper::IsSupported() {
  return supported_;
}

bool EcWakeupHelper::AllowWakeupAsTablet(bool enabled) {
  int new_wake_angle = enabled ? 360 : 180;
  std::string str = base::IntToString(new_wake_angle);
  if (new_wake_angle == cached_wake_angle_) {
    VLOG(1) << "EC wake angle is already set to " << str;
    return true;
  }
  if (base::WriteFile(sysfs_node_, str.c_str(), str.size()) < 0) {
    PLOG(ERROR) << "Failed to set EC wake angle to " << str;
    return false;
  }
  LOG(INFO) << "EC wake angle set to " << str;
  cached_wake_angle_ = new_wake_angle;
  return true;
}

}  // namespace system
}  // namespace power_manager
