// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_POWER_MANAGER_PROXY_INTERFACE_
#define SHILL_POWER_MANAGER_PROXY_INTERFACE_

#include <base/basictypes.h>

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

  // Sends a request to PowerManager to wait for this RPC client for up to
  // |delay_ms| before suspending the system. When initiating suspend,
  // PowerManager broadcasts a SuspendDelay signal and suspends the system after
  // the maximal registered timeout expires or all registered clients are ready
  // to suspend. Clients tell PowerManager that they are ready through the
  // SuspendReady signal.
  //
  // TODO(petkov): Implement SuspendReady signal sending to
  // PowerManager. Unfortunately, SuspendReady is declared as a signal when it
  // should be a method.
  virtual void RegisterSuspendDelay(uint32 delay_ms) = 0;
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
    // Place new states above kUnknown.
    kUnknown
  };

  virtual ~PowerManagerProxyDelegate() {}

  // Broadcasted by PowerManager when it initiates suspend. RPC clients that
  // have registered through RegisterSuspendDelay tell PowerManager that they're
  // ready to suspend by sending a SuspendReady signal with the same
  // |sequence_number|.
  virtual void OnSuspendDelay(uint32 sequence_number) = 0;

  // This method will be called when suspending or resuming.  |new_power_state|
  // will be "kMem" when suspending (to memory), or "kOn" when resuming.
  virtual void OnPowerStateChanged(SuspendState new_power_state) = 0;
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_PROXY_INTERFACE_
