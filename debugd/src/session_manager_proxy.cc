// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/session_manager_proxy.h"

#include <base/logging.h>

namespace debugd {

namespace session_manager_internal {

void DBusProxy::NameOwnerChanged(const std::string& name,
                                 const std::string& /*new_owner*/,
                                 const std::string& /*old_owner*/) {
  // Try to enable Chrome remote debugging again on SessionManagerServiceName
  // being owned.
  // Theoretically it should already be enabled during debugd Init().  But
  // There might be a timing issue if debugd started too fast.  We try again
  // here if the first attempt in Init() failed.
  if (name == login_manager::kSessionManagerServiceName) {
    VLOG(1) << "NameOwnerChanged: retry enable remote debugging.";
    // We need to run this async because running DBus methods inside a signal
    // handler might cause deadlock.
    session_manager_->is_session_manager_ready_ = true;
  }
}

}  // namespace session_manager_internal

void SessionManagerProxy::EnableChromeRemoteDebugging() {
  VLOG(1) << "Enable Chrome remote debugging: "
          << should_enable_chrome_remote_debugging_
          << " "
          << is_chrome_remote_debugging_enabled_;
  should_enable_chrome_remote_debugging_ = true;

  if (!dbus_proxy_) {
    dbus_proxy_.reset(new session_manager_internal::DBusProxy(&conn(), this));
  }
  EnableChromeRemoteDebuggingInternal();
}

void SessionManagerProxy::EnableChromeRemoteDebuggingInternal() {
  if (!should_enable_chrome_remote_debugging_ ||
      is_chrome_remote_debugging_enabled_) {
    return;
  }
  try {
    EnableChromeTesting(true,  // force restart Chrome
                        {"--remote-debugging-port=9222"});
    is_chrome_remote_debugging_enabled_ = true;
  } catch (DBus::Error &err) {
    LOG(ERROR) << "Failed to enable Chrome remote debugging: " << err;
  }
}

void SessionManagerProxy::MaybeRunCallback() {
  if (is_session_manager_ready_ &&
      should_enable_chrome_remote_debugging_ &&
      !is_chrome_remote_debugging_enabled_ &&
      enable_chrome_remote_debugging_try_counter_ > 0) {
    --enable_chrome_remote_debugging_try_counter_;
    EnableChromeRemoteDebuggingInternal();
  }
}

}  // namespace debugd
