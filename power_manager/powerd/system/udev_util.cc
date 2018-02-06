// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/udev_util.h"

#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/udev.h"

namespace {

// Udev device type for USB devices.
static const char kUSBDevice[] = "usb_device";

}  // namespace

namespace power_manager {
namespace system {
namespace udev_util {

bool FindWakeCapableParent(const std::string& syspath,
                           UdevInterface* udev,
                           std::string* parent_syspath_out) {
  DCHECK(udev);
  DCHECK(parent_syspath_out);
  return udev->FindParentWithSysattr(syspath, kPowerWakeup, kUSBDevice,
                                     parent_syspath_out);
}

}  // namespace udev_util
}  // namespace system
}  // namespace power_manager
