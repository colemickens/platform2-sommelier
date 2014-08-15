// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_WAKEUP_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_WAKEUP_CONTROLLER_H_

#include <string>

#include <base/basictypes.h>

#include "power_manager/powerd/system/input_observer.h"
#include "power_manager/powerd/system/udev_tagged_device_observer.h"

namespace power_manager {
namespace system {
class AcpiWakeupHelperInterface;
class InputInterface;
class TaggedDevice;
class UdevInterface;
}  // namespace system

namespace policy {

// Describes which mode the system is currently in, depending on e.g. the state
// of the lid. Currently, WakeupController only tracks CLOSED and OPEN.
enum WakeupMode {
  WAKEUP_MODE_CLOSED = 0,  // Lid closed, no external monitor attached.
  WAKEUP_MODE_DOCKED,  // Lid closed, external monitor attached.
  WAKEUP_MODE_LAPTOP,  // Lid open.
  WAKEUP_MODE_TABLET,  // Tablet mode, e.g. lid open more than 180 degrees.
};

// Configures wakeup-capable devices according to the current lid state.
class WakeupController : public system::InputObserver,
                         public system::UdevTaggedDeviceObserver {
 public:
  // Powerd tags.
  static const char kTagUsableWhenDocked[];
  static const char kTagUsableWhenLaptop[];
  static const char kTagUsableWhenTablet[];
  static const char kTagWakeup[];
  static const char kTagWakeupOnlyWhenUsable[];

  // Sysfs power/wakeup constants.
  static const char kPowerWakeup[];
  static const char kEnabled[];
  static const char kDisabled[];

  // ACPI device names.
  static const char kTPAD[];
  static const char kTSCR[];

  WakeupController();
  virtual ~WakeupController();

  void Init(system::InputInterface* input,
            system::UdevInterface* udev,
            system::AcpiWakeupHelperInterface* acpi_wakeup_helper);

  // Implementation of InputObserver.
  void OnLidEvent(LidState state) override;
  void OnPowerButtonEvent(ButtonState state) override;

  // Implementation of TaggedDeviceObserver.
  void OnTaggedDeviceChanged(const system::TaggedDevice& device) override;
  void OnTaggedDeviceRemoved(const system::TaggedDevice& device) override;

 private:
  // Enables or disables wakeup from S3 for this device (through power/wakeup).
  void SetWakeupFromS3(const system::TaggedDevice& device, bool enabled);

  // Configures wakeup for |device| according to our policy.
  void ConfigureTaggedDevice(const system::TaggedDevice& device);

  // Re-configures ACPI wakeup.
  void UpdateAcpiWakeup();

  // Re-configures all known devices to reflect a policy change.
  void PolicyChanged();

  system::InputInterface* input_;  // weak
  system::UdevInterface* udev_;  // weak
  system::AcpiWakeupHelperInterface* acpi_wakeup_helper_;  // weak

  WakeupMode mode_;

  DISALLOW_COPY_AND_ASSIGN(WakeupController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_WAKEUP_CONTROLLER_H_
