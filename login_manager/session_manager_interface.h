// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_
#define LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_

#include <string>
#include <vector>

namespace login_manager {

class SessionManagerInterface {
 public:
  SessionManagerInterface() {}
  virtual ~SessionManagerInterface() {}

  // Intializes policy subsystems.  Failure to initialize must be fatal.
  // Note: Initialize() does not start D-Bus service, yet.
  virtual bool Initialize() = 0;
  virtual void Finalize() = 0;

  // Starts SessionManagerInterface D-Bus service.
  // Returns true on success. Failure to start must be fatal.
  virtual bool StartDBusService() = 0;

  // Get Chrome startup flags from policy.
  virtual std::vector<std::string> GetStartUpFlags() = 0;

  // Emits state change signals.
  virtual void AnnounceSessionStoppingIfNeeded() = 0;
  virtual void AnnounceSessionStopped() = 0;

  // There are some times when, instead of restarting the browser on a crash,
  // the user's session should end instead (e.g. screen is currently locked).
  virtual bool ShouldEndSession() = 0;

  // Starts a 'Powerwash' of the device.  |reason| is persisted to clobber.log
  // to annotate the cause of the powerwash.  |reason| must not exceed 50 bytes
  // in length and may only contain alphanumeric characters and underscores.
  virtual void InitiateDeviceWipe(const std::string& reason) = 0;
};

}  // namespace login_manager
#endif  // LOGIN_MANAGER_SESSION_MANAGER_INTERFACE_H_
