// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_POWER_MANAGER_H_
#define WIMAX_MANAGER_POWER_MANAGER_H_

#include <string>

#include <base/basictypes.h>

#include "wimax_manager/dbus_proxiable.h"

namespace wimax_manager {

class Manager;
class PowerManagerDBusProxy;

class PowerManager : public DBusProxiable<PowerManager, PowerManagerDBusProxy> {
 public:
  explicit PowerManager(Manager *wimax_manager);
  ~PowerManager();

  void Initialize();
  void Finalize();

  void RegisterSuspendDelay(uint32 delay_ms);
  void UnregisterSuspendDelay();
  void OnSuspendDelay(uint32 sequence_number);
  void OnPowerStateChanged(const std::string &new_power_state);

 private:
  void SuspendReady(uint32 sequence_number);

  bool suspend_delay_registered_;
  bool suspended_;
  Manager *wimax_manager_;

  DISALLOW_COPY_AND_ASSIGN(PowerManager);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_NETWORK_H_
