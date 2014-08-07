// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_POWER_MANAGER_H_
#define WIMAX_MANAGER_POWER_MANAGER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/time/time.h>
#include <base/timer/timer.h>

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

  // Invoked when the power manager is about to attempt to suspend the system.
  // Prepares the manager for suspend and calls SendHandleSuspendReadiness().
  void OnSuspendImminent(const std::vector<uint8_t> &serialized_proto);

  // Invoked when the power manager has completed a suspend attempt (possibly
  // without actually suspending and resuming if the attempt was canceled by the
  // user).
  void OnSuspendDone(const std::vector<uint8_t> &serialized_proto);

 private:
  // Calls the power manager's HandleSuspendReadiness method to report readiness
  // for suspend attempt |suspend_id|.
  void SendHandleSuspendReadiness(int suspend_id);

  // Invoked by |suspend_timeout_timer_| if the power manager doesn't emit a
  // SuspendDone signal quickly enough after announcing a suspend attempt.
  void ResumeOnSuspendTimedOut();

  // Called by OnSuspendDone() and ResumeOnSuspendTimedOut() to handle the
  // completion of a suspend attempt.
  void HandleResume();

  // Is a suspend delay currently registered?
  bool suspend_delay_registered_;

  // Power-manager-assigned ID representing the delay registered by
  // RegisterSuspendDelay().
  int suspend_delay_id_;

  bool suspended_;
  base::OneShotTimer<PowerManager> suspend_timeout_timer_;
  Manager *wimax_manager_;

  DISALLOW_COPY_AND_ASSIGN(PowerManager);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_POWER_MANAGER_H_
