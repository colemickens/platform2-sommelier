// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/wifi_controller.h"

#include "power_manager/common/prefs.h"

namespace power_manager {
namespace policy {

const char WifiController::kUdevSubsystem[] = "net";
const char WifiController::kUdevDevtype[] = "wlan";

WifiController::WifiController() = default;

WifiController::~WifiController() {
  if (udev_)
    udev_->RemoveSubsystemObserver(kUdevSubsystem, this);
}

void WifiController::Init(Delegate* delegate,
                          PrefsInterface* prefs,
                          system::UdevInterface* udev,
                          TabletMode tablet_mode) {
  DCHECK(delegate);
  DCHECK(prefs);
  DCHECK(udev);

  delegate_ = delegate;
  udev_ = udev;
  tablet_mode_ = tablet_mode;

  prefs->GetBool(kSetWifiTransmitPowerForTabletModePref,
                 &set_transmit_power_for_tablet_mode_);
  udev_->AddSubsystemObserver(kUdevSubsystem, this);
  UpdateTransmitPower();
}

void WifiController::HandleTabletModeChange(TabletMode mode) {
  if (tablet_mode_ == mode)
    return;

  tablet_mode_ = mode;
  UpdateTransmitPower();
}

void WifiController::OnUdevEvent(const system::UdevEvent& event) {
  DCHECK_EQ(event.device_info.subsystem, kUdevSubsystem);
  if (event.action == system::UdevEvent::Action::ADD &&
      event.device_info.devtype == kUdevDevtype)
    UpdateTransmitPower();
}

void WifiController::UpdateTransmitPower() {
  if (set_transmit_power_for_tablet_mode_ &&
      tablet_mode_ != TabletMode::UNSUPPORTED)
    delegate_->SetWifiTransmitPower(tablet_mode_);
}

}  // namespace policy
}  // namespace power_manager
