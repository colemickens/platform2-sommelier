// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/wakeup_controller.h"

#include <base/logging.h>
#include <vector>

#include "power_manager/powerd/system/acpi_wakeup_helper.h"
#include "power_manager/powerd/system/input.h"
#include "power_manager/powerd/system/tagged_device.h"
#include "power_manager/powerd/system/udev.h"

namespace power_manager {
namespace policy {

namespace {

bool IsUsableInMode(const system::TaggedDevice& device, WakeupMode mode) {
  return ((device.HasTag(WakeupController::kTagUsableWhenDocked) &&
               mode == WAKEUP_MODE_DOCKED) ||
          (device.HasTag(WakeupController::kTagUsableWhenLaptop) &&
               mode == WAKEUP_MODE_LAPTOP) ||
          (device.HasTag(WakeupController::kTagUsableWhenTablet) &&
               mode == WAKEUP_MODE_TABLET));
}

}  // namespace

const char WakeupController::kTagUsableWhenDocked[] = "usable_when_docked";
const char WakeupController::kTagUsableWhenLaptop[] = "usable_when_laptop";
const char WakeupController::kTagUsableWhenTablet[] = "usable_when_tablet";
const char WakeupController::kTagWakeup[] = "wakeup";
const char WakeupController::kTagWakeupOnlyWhenUsable[] =
    "wakeup_only_when_usable";

const char WakeupController::kPowerWakeup[] = "power/wakeup";
const char WakeupController::kEnabled[] = "enabled";
const char WakeupController::kDisabled[] = "disabled";

const char WakeupController::kTPAD[] = "TPAD";
const char WakeupController::kTSCR[] = "TSCR";

WakeupController::WakeupController()
    : input_(NULL),
      udev_(NULL),
      acpi_wakeup_helper_(NULL),
      mode_(WAKEUP_MODE_LAPTOP) {}

WakeupController::~WakeupController() {
  if (input_)
    input_->RemoveObserver(this);
  if (udev_)
    udev_->RemoveTaggedDeviceObserver(this);
}

void WakeupController::Init(
    system::InputInterface* input,
    system::UdevInterface* udev,
    system::AcpiWakeupHelperInterface* acpi_wakeup_helper) {
  input_ = input;
  udev_ = udev;
  acpi_wakeup_helper_ = acpi_wakeup_helper;

  input_->AddObserver(this);
  udev_->AddTaggedDeviceObserver(this);

  OnLidEvent(input_->QueryLidState());
}

void WakeupController::OnLidEvent(LidState state) {
  switch (state) {
    case LID_CLOSED:
      mode_ = WAKEUP_MODE_CLOSED;
      break;

    case LID_OPEN:
    case LID_NOT_PRESENT:
      mode_ = WAKEUP_MODE_LAPTOP;
      break;
  }

  PolicyChanged();
}

void WakeupController::OnPowerButtonEvent(ButtonState state) {}

void WakeupController::OnTaggedDeviceChanged(
    const system::TaggedDevice& device) {
  ConfigureTaggedDevice(device);
}

void WakeupController::OnTaggedDeviceRemoved(
    const system::TaggedDevice& device) {}

void WakeupController::SetWakeupFromS3(const system::TaggedDevice& device,
                                       bool enabled) {
  // For USB devices, the input device does not have a power/wakeup property
  // itself, but the corresponding USB device does. If the matching device does
  // not have a power/wakeup property, we thus fall back to the first ancestor
  // that has one. Conflicts should not arise, since real-world USB input
  // devices typically only expose one input interface anyway.
  std::string parent_syspath;
  if (!udev_->FindParentWithSysattr(device.syspath(), kPowerWakeup,
                                    &parent_syspath)) {
    LOG(WARNING) << "no " << kPowerWakeup << " sysattr available for "
                 << device.syspath();
    return;
  }
  VLOG(1) << (enabled ? "Enabling" : "Disabling") << " wakeup for "
          << device.syspath() << " through " << parent_syspath;
  udev_->SetSysattr(parent_syspath,
                    kPowerWakeup,
                    enabled ? kEnabled : kDisabled);
}

void WakeupController::ConfigureTaggedDevice(
    const system::TaggedDevice& device) {
  // Do we manage wakeup for this device?
  if (!device.HasTag(kTagWakeup))
    return;

  bool wakeup = true;
  if (device.HasTag(kTagWakeupOnlyWhenUsable))
    wakeup = IsUsableInMode(device, mode_);

  SetWakeupFromS3(device, wakeup);
}

void WakeupController::UpdateAcpiWakeup() {
  // On x86 systems, setting power/wakeup in sysfs is not enough, we also need
  // to go through /proc/acpi/wakeup.

  if (!acpi_wakeup_helper_->IsSupported())
    return;

  acpi_wakeup_helper_->SetWakeupEnabled(kTPAD, mode_ == WAKEUP_MODE_LAPTOP);
  acpi_wakeup_helper_->SetWakeupEnabled(kTSCR, false);
}

void WakeupController::PolicyChanged() {
  VLOG(1) << "Policy changed, updating wakeup for existing devices";

  std::vector<system::TaggedDevice> devices = udev_->GetTaggedDevices();
  for (const system::TaggedDevice& device : devices)
    ConfigureTaggedDevice(device);

  UpdateAcpiWakeup();
}

}  // namespace policy
}  // namespace power_manager
