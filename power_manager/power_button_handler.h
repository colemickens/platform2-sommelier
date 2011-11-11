// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_BUTTON_HANDLER_H_
#define POWER_MANAGER_POWER_BUTTON_HANDLER_H_
#pragma once

#include <glib.h>

#include "base/basictypes.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "power_manager/signal_callback.h"

namespace power_manager {

class Daemon;

// PowerButtonHandler locks the screen and shuts down the system in response to
// the power button being held down.
class PowerButtonHandler {
 public:
  explicit PowerButtonHandler(Daemon* daemon);
  ~PowerButtonHandler();

  // Handle the power button being pressed or released.
  void HandlePowerButtonDown();
  void HandlePowerButtonUp();

  // Handle the lock button being pressed or released.
  void HandleLockButtonDown();
  void HandleLockButtonUp();

  // Handle notification from Chrome that the screen has been locked.
  void HandleScreenLocked();

 private:
  // Lock the screen and add a timeout for OnLockToShutdownTimeout().
  SIGNAL_CALLBACK_0(PowerButtonHandler, gboolean, OnLockTimeout);

  // The power button has been held continuously through the unlocked and locked
  // states, and has been down for long enough that we're considering shutting
  // down the machine.  Starts the shutdown timeout.
  SIGNAL_CALLBACK_0(PowerButtonHandler, gboolean, OnLockToShutdownTimeout);

  // The power button has been held continuously through the unlocked state
  // but we have not been ack'd by the screen lock. Starts the shutdown timeout
  // to ignore the nonresponsive screenlocker.
  SIGNAL_CALLBACK_0(PowerButtonHandler, gboolean, OnLockFailTimeout);

  // Tell the window manager to start playing the shutdown animation and add a
  // timeout for OnRealShutdownTimeout() to fire after the animation is done.
  SIGNAL_CALLBACK_0(PowerButtonHandler, gboolean, OnShutdownTimeout);

  // Dim the backlight and actually shut down the machine.
  SIGNAL_CALLBACK_0(PowerButtonHandler, gboolean, OnRealShutdownTimeout)

  // Tell the window manager to start the pre-shutdown animation and add a
  // timeout for HandleShutdownTimeout().
  void AddShutdownTimeout();

  // Send an X ClientEvent message to the window manager notifying it about the
  // state of the power button.  Returns false if we were unable to send the
  // message.
  bool NotifyWindowManagerAboutPowerButtonState(
      chromeos::WmIpcPowerButtonState button_state);

  // Send an X ClientEvent message to the window manager notifying it that the
  // system is being shut down.  Returns false if we were unable to send the
  // message.
  bool NotifyWindowManagerAboutShutdown();

  Daemon* daemon_;  // not owned

  // Timeouts for calling the corresponding Handle*TimeoutThunk() methods.
  // 0 if unregistered.
  guint lock_timeout_id_;
  guint lock_to_shutdown_timeout_id_;
  guint lock_fail_timeout_id_;
  guint shutdown_timeout_id_;
  guint real_shutdown_timeout_id_;
  bool lock_button_down_;
  bool power_button_down_;

  // Are we in the process of shutting down the system?
  bool shutting_down_;

  // Should we call AddShutdownTimeout() when we receive notification that the
  // screen has been locked?
  bool should_add_shutdown_timeout_after_lock_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonHandler);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_BUTTON_HANDLER_H_
