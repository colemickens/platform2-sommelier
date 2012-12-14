// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_POWER_MANAGER_PROXY_INTERFACE_H_
#define SHILL_POWER_MANAGER_PROXY_INTERFACE_H_

#include <base/basictypes.h>
#include <base/time.h>

namespace shill {

// This class provides events from the power manager.  To use this class, create
// a subclass from PowerManagerProxyDelegate and implement its member functions.
// Call ProxyFactory::CreatePowerManagerProxy() to create an instance of this
// proxy, passing it a pointer to the delegate you created.  When an event from
// the power manager is received, your delegate's member function will be
// called. You retain ownership of the delegate and must ensure that the proxy
// is deleted before the delegate.
class PowerManagerProxyInterface {
 public:
  virtual ~PowerManagerProxyInterface() {}

  // Sends a request to the power manager to wait for this client for up to
  // |timeout| before suspending the system.  Assigns an ID corresponding to the
  // registered delay to |delay_id_out| and returns true on success.
  virtual bool RegisterSuspendDelay(base::TimeDelta timeout,
                                    int *delay_id_out) = 0;

  // Unregisters a previously-registered suspend delay.  Returns true on
  // success.
  virtual bool UnregisterSuspendDelay(int delay_id) = 0;

  // Calls the power manager's HandleSuspendReadiness method.  |delay_id| should
  // contain the ID returned via RegisterSuspendDelay() and |suspend_id| should
  // contain the ID from OnSuspendImminent().  Returns true on success.
  virtual bool ReportSuspendReadiness(int delay_id, int suspend_id) = 0;
};

// PowerManager signal delegate to be associated with the proxy.
class PowerManagerProxyDelegate {
 public:
  // Possible states broadcast from the powerd_suspend script.
  enum SuspendState {
    kOn,
    kStandby,
    kMem,
    kDisk,
    kSuspending,  // Internal to shill.
    // Place new states above kUnknown.
    kUnknown
  };

  virtual ~PowerManagerProxyDelegate() {}

  // Broadcasted by the power manager when it's about to suspend. RPC clients
  // that have registered through RegisterSuspendDelay() should tell the power
  // manager that they're ready to suspend by calling ReportSuspendReadiness()
  // with the delay ID returned by RegisterSuspendDelay() and |suspend_id|.
  virtual void OnSuspendImminent(int suspend_id) = 0;

  // This method will be called when suspending or resuming.  |new_power_state|
  // will be "kMem" when suspending (to memory), or "kOn" when resuming.
  virtual void OnPowerStateChanged(SuspendState new_power_state) = 0;
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_PROXY_INTERFACE_H_
