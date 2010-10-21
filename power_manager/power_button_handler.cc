// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_button_handler.h"

#include <gdk/gdkx.h>

#include "base/logging.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/powerd.h"
#include "power_manager/screen_locker.h"
#include "power_manager/util.h"

namespace power_manager {

// Amount of time that the power button needs to be held before we lock the
// screen.
static const guint kLockTimeoutMs = 400;

// Amount of time that the power button needs to be held before we shut down.
static const guint kShutdownTimeoutMs = 400;

// When the button has been held continuously from the unlocked state, amount of
// time that we wait after locking the screen before starting the pre-shutdown
// animation.
static const guint kLockToShutdownTimeoutMs = 600;

// Amount of time that we give the window manager to display the shutdown
// animation before we dim the screen and start actually shutting down the
// system.
static const guint kShutdownAnimationMs = 150;

// Name of the X selection that the window manager takes ownership of.  This
// comes from ICCCM 4.3; see http://tronche.com/gui/x/icccm/sec-4.html#s-4.3.
static const char kWindowManagerSelectionName[] = "WM_S0";

// Name of the atom used in the message_type field of X ClientEvent messages
// sent to the Chrome OS window manager.  This is hardcoded in the window
// manager.
static const char kWindowManagerMessageTypeName[] = "_CHROME_WM_MESSAGE";

// If the ID pointed to by |timeout_id| is non-zero, remove the timeout and set
// the memory to 0.
static void RemoveTimeoutIfSet(guint* timeout_id) {
  if (timeout_id) {
    g_source_remove(*timeout_id);
    *timeout_id = 0;
  }
}

PowerButtonHandler::PowerButtonHandler(Daemon* daemon)
    : daemon_(daemon),
      lock_timeout_id_(0),
      lock_to_shutdown_timeout_id_(0),
      shutdown_timeout_id_(0),
      real_shutdown_timeout_id_(0),
      shutting_down_(false) {
}

PowerButtonHandler::~PowerButtonHandler() {
  RemoveTimeoutIfSet(&lock_timeout_id_);
  RemoveTimeoutIfSet(&lock_to_shutdown_timeout_id_);
  RemoveTimeoutIfSet(&shutdown_timeout_id_);
  RemoveTimeoutIfSet(&real_shutdown_timeout_id_);
}

void PowerButtonHandler::HandleButtonDown() {
  if (shutting_down_)
    return;

#ifdef NEW_POWER_BUTTON
  // Power button release supported. This allows us to schedule events based
  // on how long the button was held down.
  if (util::LoggedIn() && !daemon_->locker()->is_locked()) {
    NotifyWindowManagerAboutPowerButtonState(
        chromeos::WM_IPC_POWER_BUTTON_PRE_LOCK);
    RemoveTimeoutIfSet(&lock_timeout_id_);
    lock_timeout_id_ =
        g_timeout_add(kLockTimeoutMs,
                      PowerButtonHandler::HandleLockTimeoutThunk,
                      this);
  } else {
    AddShutdownTimeout();
  }
#else
  // Legacy behavior for x86 systems because the ACPI button driver sends both
  // down and release events at the time the acpi notify occurs for power
  // button.
  if (util::LoggedIn() && !daemon_->locker()->is_locked())
    daemon_->locker()->LockScreen();
  else
    HandleShutdownTimeout();
#endif
}

void PowerButtonHandler::HandleButtonUp() {
  if (shutting_down_)
    return;

#ifdef NEW_POWER_BUTTON
  if (lock_timeout_id_) {
    RemoveTimeoutIfSet(&lock_timeout_id_);
    NotifyWindowManagerAboutPowerButtonState(
        chromeos::WM_IPC_POWER_BUTTON_ABORTED_LOCK);
  }
  if (shutdown_timeout_id_) {
    RemoveTimeoutIfSet(&shutdown_timeout_id_);
    NotifyWindowManagerAboutPowerButtonState(
        chromeos::WM_IPC_POWER_BUTTON_ABORTED_SHUTDOWN);
  }
  RemoveTimeoutIfSet(&lock_to_shutdown_timeout_id_);
#endif
}

void PowerButtonHandler::HandleLockTimeout() {
  lock_timeout_id_ = 0;
  daemon_->locker()->LockScreen();
  RemoveTimeoutIfSet(&lock_to_shutdown_timeout_id_);
  lock_to_shutdown_timeout_id_ =
      g_timeout_add(kLockToShutdownTimeoutMs,
                    PowerButtonHandler::HandleLockToShutdownTimeoutThunk,
                    this);
}

void PowerButtonHandler::HandleLockToShutdownTimeout() {
  lock_to_shutdown_timeout_id_ = 0;
  AddShutdownTimeout();
}

void PowerButtonHandler::HandleShutdownTimeout() {
  shutdown_timeout_id_ = 0;
  shutting_down_ = true;
  NotifyWindowManagerAboutShutdown();
  DCHECK(real_shutdown_timeout_id_ == 0) << "Shutdown already in-progress";
  real_shutdown_timeout_id_ =
      g_timeout_add(kShutdownAnimationMs,
                    PowerButtonHandler::HandleRealShutdownTimeoutThunk,
                    this);
}

void PowerButtonHandler::HandleRealShutdownTimeout() {
  real_shutdown_timeout_id_ = 0;
  // TODO: Ideally, we'd use the backlight controller to turn off the display
  // after the window manager has had enough time to display the shutdown
  // animation.  Using DPMS for this is pretty ugly, though -- the backlight
  // turns back on when X exits or if the user moves the mouse or hits a key.
  // We just dim it instead for now.
  daemon_->backlight_controller()->SetDimState(BACKLIGHT_DIM);
  daemon_->OnRequestShutdown();
}

void PowerButtonHandler::AddShutdownTimeout() {
  NotifyWindowManagerAboutPowerButtonState(
      chromeos::WM_IPC_POWER_BUTTON_PRE_SHUTDOWN);
  RemoveTimeoutIfSet(&shutdown_timeout_id_);
  shutdown_timeout_id_ =
      g_timeout_add(kShutdownTimeoutMs,
                    PowerButtonHandler::HandleShutdownTimeoutThunk,
                    this);
}

bool PowerButtonHandler::NotifyWindowManagerAboutPowerButtonState(
    chromeos::WmIpcPowerButtonState button_state) {
  return SendMessageToWindowManager(
             chromeos::WM_IPC_MESSAGE_WM_NOTIFY_POWER_BUTTON_STATE,
             button_state);
}

bool PowerButtonHandler::NotifyWindowManagerAboutShutdown() {
  return SendMessageToWindowManager(
             chromeos::WM_IPC_MESSAGE_WM_NOTIFY_SHUTTING_DOWN, 0);
}

bool PowerButtonHandler::SendMessageToWindowManager(
    chromeos::WmIpcMessageType type, int first_param) {
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
  event.xclient.data.l[0] = type;
  event.xclient.data.l[1] = first_param;
  for (int i = 2; i < 5; ++i)
    event.xclient.data.l[i] = 0;
  XSendEvent(display,
             wm_window,
             False,  // propagate
             0,      // empty event mask
             &event);

  gdk_flush();
  if (gdk_error_trap_pop()) {
    LOG(WARNING) << "Got error while sending message to window manager";
    return false;
  }
  return true;
}

}  // namespace power_manager
