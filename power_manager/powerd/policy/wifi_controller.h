// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_WIFI_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_WIFI_CONTROLLER_H_

#include <string>

#include <base/macros.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/udev.h"
#include "power_manager/powerd/system/udev_subsystem_observer.h"

namespace power_manager {

class PrefsInterface;

namespace policy {

// WifiController initiates power-related changes to the wifi chipset.
class WifiController : public system::UdevSubsystemObserver {
 public:
  // Performs work on behalf of WifiController.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Updates the wifi transmit power to |power|.
    virtual void SetWifiTransmitPower(RadioTransmitPower power) = 0;
  };

  // Net subsystem and wlan devtype for udev events.
  static const char kUdevSubsystem[];
  static const char kUdevDevtype[];

  WifiController();
  ~WifiController() override;

  // Ownership of raw pointers remains with the caller.
  void Init(Delegate* delegate,
            PrefsInterface* prefs,
            system::UdevInterface* udev,
            TabletMode tablet_mode);

  // Called when the tablet mode changes.
  void HandleTabletModeChange(TabletMode mode);

  // system::UdevSubsystemObserver:
  void OnUdevEvent(const system::UdevEvent& event) override;

 private:
  // Updates transmit power via |delegate_| if
  // |set_transmit_power_for_tablet_mode_| is true.
  void UpdateTransmitPower();

  Delegate* delegate_ = nullptr;           // Not owned.
  system::UdevInterface* udev_ = nullptr;  // Not owned.

  TabletMode tablet_mode_ = TabletMode::UNSUPPORTED;

  // True if powerd has been configured to set wifi transmit power in response
  // to tablet mode changes.
  bool set_transmit_power_for_tablet_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(WifiController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_WIFI_CONTROLLER_H_
