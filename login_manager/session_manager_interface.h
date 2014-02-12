// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_
#define LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_

#include <string>
#include <vector>

#include <base/basictypes.h>

namespace login_manager {

class SessionManagerInterface {
 public:
  SessionManagerInterface() {}
  virtual ~SessionManagerInterface() {}

  // Intializes policy subsystems.  Failure to initialize must be fatal.
  virtual bool Initialize() = 0;
  virtual void Finalize() = 0;

  // Get Chrome startup flags from policy.
  virtual std::vector<std::string> GetStartUpFlags() = 0;

  // Emits state change signals.
  virtual void AnnounceSessionStoppingIfNeeded() = 0;
  virtual void AnnounceSessionStopped() = 0;

  virtual bool ScreenIsLocked() = 0;

    // Starts a 'Powerwash' of the device.
  virtual void InitiateDeviceWipe() = 0;
};

}  // namespace login_manager
#endif  // LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_
