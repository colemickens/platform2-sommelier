// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_POWER_MANAGER_PROXY_INTERFACE_
#define SHILL_POWER_MANAGER_PROXY_INTERFACE_

#include <base/basictypes.h>

namespace shill {

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
  virtual ~PowerManagerProxyDelegate() {}

  // Broadcasted by PowerManager when it initiates suspend. RPC clients that
  // have registered through RegisterSuspendDelay tell PowerManager that they're
  // ready to suspend by sending a SuspendReady signal with the same
  // |sequence_number|.
  virtual void OnSuspendDelay(uint32 sequence_number) = 0;
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_PROXY_INTERFACE_
