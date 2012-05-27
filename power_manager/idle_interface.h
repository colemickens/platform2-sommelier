// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_IDLE_INTERFACE_H_
#define POWER_MANAGER_IDLE_INTERFACE_H_

#include "base/basictypes.h"

namespace power_manager {

// Interface for observing idle events.
class IdleObserver {
 public:
  // Idle event handler. This handler should be called when the user either
  // becomes newly idle (due to exceeding an idle timeout) or is no longer
  // idle.
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms) = 0;

 protected:
  virtual ~IdleObserver() {}
};

// Interface for creating and watching idle timeout events.
class IdleInterface {
 public:
  IdleInterface() {}
  virtual ~IdleInterface() {}

  // Initialize the object with the given |observer|.
  // On success, return true; otherwise return false.
  virtual bool Init(IdleObserver* observer) = 0;

  // Add a timeout value. Idle events will be fired every time
  // the user either becomes newly idle (due to exceeding an idle
  // timeout) or is no longer idle.
  //
  // On success, return true; otherwise return false.
  virtual bool AddIdleTimeout(int64 idle_timeout_ms) = 0;

  // Set *idle_time_ms to how long the user has been idle, in milliseconds.
  // On success, return true; otherwise return false.
  virtual bool GetIdleTime(int64* idle_time_ms) = 0;

  // Clear all timeouts.
  // On success, return true; otherwise return false.
  virtual bool ClearTimeouts() = 0;

  DISALLOW_COPY_AND_ASSIGN(IdleInterface);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_IDLE_INTERFACE_H_
