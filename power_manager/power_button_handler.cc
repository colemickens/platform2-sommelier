// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_button_handler.h"

#include "base/logging.h"
#include "power_manager/powerd.h"
#include "power_manager/screen_locker.h"
#include "power_manager/util.h"

namespace power_manager {

// Amount of time that the power button needs to be held before we shut down.
static const guint kShutdownTimeoutMs = 1000;

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

// TODO(derat): Once we have boards that report power button releases correctly
// (as opposed to just sending them immediately after the press event), define
// this appropriately in the SConstruct file.
#ifdef POWER_BUTTON_RELEASES_ARE_REPORTED
  DCHECK(!shutdown_timeout_id_);
  shutdown_timeout_id_ =
      g_timeout_add(kShutdownTimeoutMs,
                    PowerButtonHandler::HandleShutdownTimeoutThunk,
                    this);
#else
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
  daemon_->StartCleanShutdown();
}

}  // namespace power_manager
