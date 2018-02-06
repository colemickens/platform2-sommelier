// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_WAKEUP_DEVICE_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_WAKEUP_DEVICE_INTERFACE_H_

#include <memory>

#include <base/files/file_path.h>

namespace power_manager {
namespace system {

// Per device object that helps in identifying if this device is one of the
// reasons for last wake.
class WakeupDeviceInterface {
 public:
  WakeupDeviceInterface() {}
  virtual ~WakeupDeviceInterface() {}

  // Records wakeup_count before suspending to identify if the
  // device woke up the system after resume.
  virtual void PrepareForSuspend() = 0;

  // Reads wakeup_count after resume and compares it to the wakeup_count
  // before suspending.
  virtual void HandleResume() = 0;

  // Returns true if the wakeup_count changed during last suspend/resume cycle.
  virtual bool CausedLastWake() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WakeupDeviceInterface);
};

class WakeupDeviceFactoryInterface {
 public:
  WakeupDeviceFactoryInterface() {}
  virtual ~WakeupDeviceFactoryInterface() {}

  // Creates a wakeup device with sysfs directory pointed by |path|.
  // Note that the directory pointed by |path| should contain power/wakeup
  // file (present only if the device is wake capable).
  // Example: /sys/devices/pci0000:00/0000:00:14.0/usb1/1-2/
  // Returns nullptr if the device isn't wake-capable.
  virtual std::unique_ptr<WakeupDeviceInterface> CreateWakeupDevice(
      const base::FilePath& path) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WakeupDeviceFactoryInterface);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_WAKEUP_DEVICE_INTERFACE_H_
