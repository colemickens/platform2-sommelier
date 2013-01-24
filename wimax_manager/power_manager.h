// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_POWER_MANAGER_H_
#define WIMAX_MANAGER_POWER_MANAGER_H_

#include <glib.h>

#include <string>

#include <base/basictypes.h>
#include <base/time.h>

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

  void ResumeOnSuspendTimedOut();

  // Synchronously registers a suspend delay with the power manager, assigning
  // the delay's ID to |suspend_delay_id_| and setting
  // |suspend_delay_registered_| to true on success.  |timeout| is the maximum
  // amount of time the power manager will wait for the WiMAX manager to
  // announce its readiness before suspending the system.  |description| is
  // a human-readable string describing the delay's purpose.
  void RegisterSuspendDelay(base::TimeDelta timeout,
                            const std::string &description);

  // Unregisters |suspend_delay_id_|.
  void UnregisterSuspendDelay();

  // Invoked when the power manager is about to suspend the system.  Prepares
  // the manager for suspend and calls SendHandleSuspendReadiness().
  void OnSuspendImminent(const std::vector<uint8> &serialized_proto);

  void OnPowerStateChanged(const std::string &new_power_state);

 private:
  void CancelSuspendTimeout();

  // Calls the power manager's HandleSuspendReadiness method to report readiness
  // for suspend attempt |suspend_id|.
  void SendHandleSuspendReadiness(int suspend_id);

  // Is a suspend delay currently registered?
  bool suspend_delay_registered_;

  // Power-manager-assigned ID representing the delay registered by
  // RegisterSuspendDelay().
  int suspend_delay_id_;

  bool suspended_;
  guint suspend_timeout_id_;
  Manager *wimax_manager_;

  DISALLOW_COPY_AND_ASSIGN(PowerManager);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_NETWORK_H_
