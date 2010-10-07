// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_BUTTON_HANDLER_H_
#define POWER_BUTTON_HANDLER_H_

#include <glib.h>

#include "base/basictypes.h"

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
  // Different states that we can be in due to the state of the power
  // button.  These are listed in the order in which we transition through
  // them if the user holds down the power button from an unlocked state.
  enum State {
    // The power button is up (the screen may be either locked or unlocked).
    kStateUnpressed = 0,

    // The power button is down but hasn't been held long enough to shut down.
    kStateWaitingForRelease,

    // We're shutting down the system.
    kStateShuttingDown,
  };

  // Cancel |shutdown_timeout_id_| if it's in-progress.
  void CancelShutdownTimeout();

  // Shut down the machine.
  static gboolean HandleShutdownTimeoutThunk(gpointer data) {
    reinterpret_cast<PowerButtonHandler*>(data)->HandleShutdownTimeout();
    return FALSE;
  }
  void HandleShutdownTimeout();

  Daemon* daemon_;  // not owned

  // The state that we're currently in.
  State state_;

  // Timeout for calling HandleShutdownTimeoutThunk(), or 0 if not running.
  guint shutdown_timeout_id_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonHandler);
};

}  // namespace power_manager

#endif
