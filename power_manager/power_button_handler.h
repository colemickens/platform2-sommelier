// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_BUTTON_HANDLER_H_
#define POWER_BUTTON_HANDLER_H_

#include <glib.h>

#include "base/basictypes.h"
#include "cros/chromeos_wm_ipc_enums.h"

namespace power_manager {

class Daemon;

// PowerButtonHandler locks the screen and shuts down the system in response to
// the power button being held down.
class PowerButtonHandler {
 public:
  explicit PowerButtonHandler(Daemon* daemon);
  ~PowerButtonHandler();

  // Handle the power button being pressed or released.
  void HandleButtonDown();
  void HandleButtonUp();

 private:
  // Lock the screen and add a timeout for HandleLockToShutdownTimeout().
  static gboolean HandleLockTimeoutThunk(gpointer data) {
    reinterpret_cast<PowerButtonHandler*>(data)->HandleLockTimeout();
    return FALSE;
  }
  void HandleLockTimeout();

  // The power button has been held continuously through the unlocked and locked
  // states, and has been down for long enough that we're considering shutting
  // down the machine.  Starts the shutdown timeout.
  static gboolean HandleLockToShutdownTimeoutThunk(gpointer data) {
    reinterpret_cast<PowerButtonHandler*>(data)->HandleLockToShutdownTimeout();
    return FALSE;
  }
  void HandleLockToShutdownTimeout();

  // Tell the window manager to start playing the shutdown animation and add a
  // timeout for HandleRealShutdownTimeout() to fire after the animation is
  // done.
  static gboolean HandleShutdownTimeoutThunk(gpointer data) {
    reinterpret_cast<PowerButtonHandler*>(data)->HandleShutdownTimeout();
    return FALSE;
  }
  void HandleShutdownTimeout();

  // Dim the backlight and actually shut down the machine.
  static gboolean HandleRealShutdownTimeoutThunk(gpointer data) {
    reinterpret_cast<PowerButtonHandler*>(data)->HandleRealShutdownTimeout();
    return FALSE;
  }
  void HandleRealShutdownTimeout();

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

  // Helper method used by NotifyWindowManagerAboutPowerButtonState() and
  // NotifyWindowManagerAboutShutdown().
  bool SendMessageToWindowManager(chromeos::WmIpcMessageType type,
                                  int first_param);

  Daemon* daemon_;  // not owned

  // Timeouts for calling the corresponding Handle*TimeoutThunk() methods.
  // 0 if unregistered.
  guint lock_timeout_id_;
  guint lock_to_shutdown_timeout_id_;
  guint shutdown_timeout_id_;
  guint real_shutdown_timeout_id_;

  // Are we in the process of shutting down the system?
  bool shutting_down_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonHandler);
};

}  // namespace power_manager

#endif
