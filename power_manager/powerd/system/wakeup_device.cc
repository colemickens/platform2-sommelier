// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/wakeup_device.h"

#include <string>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/string_number_conversions.h>

#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace system {

// static
std::unique_ptr<WakeupDeviceInterface> WakeupDevice::CreateWakeupDevice(
    const base::FilePath& path) {
  const base::FilePath wakeup_path = path.Append(kPowerWakeup);
  if (!base::PathExists(wakeup_path)) {
    // This can happen when the device is not wake capable.
    return nullptr;
  }
  return std::unique_ptr<WakeupDevice>(new WakeupDevice(path));
}

// static
const char WakeupDevice::kPowerWakeupCount[] = "power/wakeup_count";

WakeupDevice::WakeupDevice(const base::FilePath& path) : sys_path_(path) {}

WakeupDevice::~WakeupDevice() {}

void WakeupDevice::PrepareForSuspend() {
  // This can happen when the device is no more a wake source (if power/wakeup
  // is disabled).
  was_pre_suspend_read_successful_ =
      ReadWakeupCount(&wakeup_count_before_suspend_);
}

void WakeupDevice::HandleResume() {
  caused_last_wake_ = false;
  if (!was_pre_suspend_read_successful_) {
    return;
  }

  uint64_t wakeup_count_after_resume = 0;

  // This can happen when the device is no more a wake source (if power/wakeup
  // is disabled).
  if (!ReadWakeupCount(&wakeup_count_after_resume))
    return;

  if (wakeup_count_after_resume != wakeup_count_before_suspend_) {
    LOG(INFO) << "Device " << sys_path_.value() << " had wakeup count "
              << wakeup_count_before_suspend_ << " before suspend and "
              << wakeup_count_after_resume << " after resume";
    caused_last_wake_ = true;
  }
}

bool WakeupDevice::CausedLastWake() const {
  return caused_last_wake_;
}

bool WakeupDevice::ReadWakeupCount(uint64_t* wakeup_count_out) {
  DCHECK(wakeup_count_out);
  std::string wakeup_count_str;

  const base::FilePath wakeup_count_path = sys_path_.Append(kPowerWakeupCount);
  // This can happen if the device is not wake capable anymore.
  if (!base::PathExists(wakeup_count_path))
    return false;

  if (!base::ReadFileToString(wakeup_count_path, &wakeup_count_str)) {
    PLOG(ERROR) << "Unable to read wakeup count for " << sys_path_.value();
    return false;
  }
  // Some drivers leave the wakeup_count empty initially.
  if (wakeup_count_str.empty()) {
    *wakeup_count_out = 0;
    return true;
  }
  base::TrimWhitespaceASCII(wakeup_count_str, base::TRIM_TRAILING,
                            &wakeup_count_str);
  if (base::StringToUint64(wakeup_count_str, wakeup_count_out)) {
    return true;
  }
  LOG(ERROR) << "Could not parse wakeup_count sysattr for "
             << sys_path_.value();
  return false;
}

}  // namespace system
}  // namespace power_manager
