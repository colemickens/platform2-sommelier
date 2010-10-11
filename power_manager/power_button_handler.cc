// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_button_handler.h"

#include <gdk/gdkx.h>

#include "base/logging.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/powerd.h"
#include "power_manager/screen_locker.h"
#include "power_manager/util.h"

namespace power_manager {

// Amount of time that the power button needs to be held before we shut down.
static const guint kShutdownTimeoutMs = 1000;

// Amount of time that we wait for the window manager to display the shutdown
// animation before dimming the backlight.
static const guint kDimBacklightTimeoutMs = 150;

// Name of the X selection that the window manager takes ownership of.  This
// comes from ICCCM 4.3; see http://tronche.com/gui/x/icccm/sec-4.html#s-4.3.
static const char kWindowManagerSelectionName[] = "WM_S0";

// Name of the atom used in the message_type field of X ClientEvent messages
// sent to the Chrome OS window manager.  This is hardcoded in the window
// manager.
static const char kWindowManagerMessageTypeName[] = "_CHROME_WM_MESSAGE";

PowerButtonHandler::PowerButtonHandler(Daemon* daemon)
    : daemon_(daemon),
      state_(kStateUnpressed),
      shutdown_timeout_id_(0) {
}

PowerButtonHandler::~PowerButtonHandler() {
  CancelShutdownTimeout();
}

void PowerButtonHandler::HandleButtonDown() {
  if (state_ != kStateUnpressed)
    return;

  state_ = kStateWaitingForRelease;

#ifdef NEW_POWER_BUTTON
  // Power button release supported. This allows us to schedule events based
  // on how long the button was held down.
  DCHECK(!shutdown_timeout_id_);
  shutdown_timeout_id_ =
      g_timeout_add(kShutdownTimeoutMs,
                    PowerButtonHandler::HandleShutdownTimeoutThunk,
                    this);
#else
  // Legacy behavior for x86 systems because the ACPI button driver sends both
  // down and release events at the time the acpi notify occurs for power
  // button.
  if (!util::LoggedIn() || daemon_->locker()->is_locked())
    HandleShutdownTimeout();
  else
    daemon_->locker()->LockScreen();
#endif
}

void PowerButtonHandler::HandleButtonUp() {
  if (state_ != kStateWaitingForRelease)
    return;

  CancelShutdownTimeout();
  state_ = kStateUnpressed;

  if (!util::LoggedIn() || daemon_->locker()->is_locked())
    return;

  daemon_->locker()->LockScreen();
}

void PowerButtonHandler::CancelShutdownTimeout() {
  if (shutdown_timeout_id_) {
    g_source_remove(shutdown_timeout_id_);
    shutdown_timeout_id_ = 0;
  }
}

void PowerButtonHandler::HandleShutdownTimeout() {
  DCHECK_EQ(state_, kStateWaitingForRelease);
  state_ = kStateShuttingDown;
  bool notified_wm = NotifyWindowManagerAboutShutdown();
  daemon_->StartCleanShutdown();
  if (notified_wm) {
    // TODO: Ideally, we'd use the backlight controller to turn off the display
    // after the window manager has had enough time to display the shutdown
    // animation.  Using DPMS for this is pretty ugly, though -- the backlight
    // turns back on when X exits or if the user moves the mouse or hits a key.
    // We just dim it instead for now.
    g_timeout_add(kDimBacklightTimeoutMs,
                  PowerButtonHandler::HandleDimBacklightTimeoutThunk,
                  this);
  }
}

void PowerButtonHandler::HandleDimBacklightTimeout() {
  daemon_->backlight_controller()->SetDimState(BACKLIGHT_DIM);
}

bool PowerButtonHandler::NotifyWindowManagerAboutShutdown() {
  // Ensure that we won't crash if we get an error from the X server.
  gdk_error_trap_push();

  Display* display = GDK_DISPLAY();
  Window wm_window = XGetSelectionOwner(
      display, XInternAtom(display, kWindowManagerSelectionName, True));
  if (!wm_window) {
    LOG(WARNING) << "Unable to find window owning the "
                 << kWindowManagerSelectionName << " X selection -- is the "
                 << "window manager running?";
    gdk_error_trap_pop();
    return false;
  }

  XEvent event;
  event.xclient.type = ClientMessage;
  event.xclient.window = wm_window;
  event.xclient.message_type =
      XInternAtom(display, kWindowManagerMessageTypeName, True);
  event.xclient.format = 32;  // 32-bit values
  event.xclient.data.l[0] = chromeos::WM_IPC_MESSAGE_WM_NOTIFY_SHUTTING_DOWN;
  for (int i = 1; i < 5; ++i)
    event.xclient.data.l[i] = 0;
  XSendEvent(display,
             wm_window,
             False,  // propagate
             0,      // empty event mask
             &event);

  gdk_flush();
  if (gdk_error_trap_pop()) {
    LOG(WARNING) << "Got error while notifying window manager about shutdown";
    return false;
  }
  return true;
}

}  // namespace power_manager
