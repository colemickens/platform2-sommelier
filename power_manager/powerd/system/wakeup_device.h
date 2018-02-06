// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_WAKEUP_DEVICE_H_
#define POWER_MANAGER_POWERD_SYSTEM_WAKEUP_DEVICE_H_

#include "power_manager/powerd/system/wakeup_device_interface.h"

#include <memory>

#include <base/files/file_path.h>

namespace power_manager {
namespace system {

class UdevInterface;

class WakeupDevice : public WakeupDeviceInterface {
 public:
  // Relative path to device specific wakeup_count from the device sysfs
  // path (power/wakeup_count).
  static const char kPowerWakeupCount[];

  WakeupDevice(const base::FilePath& path, UdevInterface* udev);
  ~WakeupDevice() override;

  bool was_pre_suspend_read_successful() const {
    return was_pre_suspend_read_successful_;
  }
  uint64_t wakeup_count_before_suspend() const {
    return wakeup_count_before_suspend_;
  }

  // Implementation of WakeupDeviceInterface.
  void PrepareForSuspend() override;
  void HandleResume() override;
  bool CausedLastWake() const override;

 private:
  // Reads the |kPowerWakeupCount|. |wakeup_count_out| is set to the read value
  // if the read is successful. Returns true on success, false otherwise.
  bool ReadWakeupCount(uint64_t* wakeup_count_out);

  // Sysfs path of the directory that hold the device specific power controls.
  base::FilePath path_;

  UdevInterface* udev_;  // weak

  // Did this device cause last wake?
  bool caused_last_wake_ = false;

  // Wakeup count of the device before the last suspend.
  uint64_t wakeup_count_before_suspend_ = 0;

  // Was wakeup_count read before suspend successful?
  bool was_pre_suspend_read_successful_ = false;

  DISALLOW_COPY_AND_ASSIGN(WakeupDevice);
};

class WakeupDeviceFactory : public WakeupDeviceFactoryInterface {
 public:
  explicit WakeupDeviceFactory(UdevInterface* udev);
  virtual ~WakeupDeviceFactory();

  // Implementation of WakeupDeviceFactoryInterface.
  std::unique_ptr<WakeupDeviceInterface> CreateWakeupDevice(
      const base::FilePath& path) override;

 private:
  UdevInterface* udev_;  // weak

  DISALLOW_COPY_AND_ASSIGN(WakeupDeviceFactory);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_WAKEUP_DEVICE_H_
