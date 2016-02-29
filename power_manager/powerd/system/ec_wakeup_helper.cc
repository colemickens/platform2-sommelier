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

const char kWakeupAngleIsSupportedPath[] =
    "/lib/udev/rules.d/99-cros-ec-accel.rules";

}  // namespace

EcWakeupHelper::EcWakeupHelper() :
    supported_(base::PathExists(base::FilePath(kWakeupAngleIsSupportedPath))),
    cached_wake_angle_(-1) {}

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
  int ret = util::RunSetuidHelper("wake_angle", "--wake_angle=" + str, true);
  if (ret != 0) {
    LOG(ERROR) << "Failed to set EC wake angle to " << str
               << ", ectool error code: " << ret;
    return false;
  }
  cached_wake_angle_ = new_wake_angle;
  return true;
}

}  // namespace system
}  // namespace power_manager
