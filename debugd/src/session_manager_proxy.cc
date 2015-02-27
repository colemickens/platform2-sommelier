// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/session_manager_proxy.h"

#include <base/logging.h>

namespace debugd {

void SessionManagerProxy::LoginPromptVisible() {
  // Try to enable Chrome remote debugging again on Login prompt.
  // Theoretically it should already be enabled during debugd Init().  But
  // There might be a timing issue if debugd started too fast.  We try again
  // here if the first attempt in Init() failed.
  EnableChromeRemoteDebuggingInternal();
}

void SessionManagerProxy::EnableChromeRemoteDebugging() {
  VLOG(1) << "Enable Chrome remote debugging: "
          << should_enable_chrome_remote_debugging_
          << " "
          << is_chrome_remote_debugging_enabled_;
  should_enable_chrome_remote_debugging_ = true;
  EnableChromeRemoteDebuggingInternal();
}

void SessionManagerProxy::EnableChromeRemoteDebuggingInternal() {
  VLOG(1) << "Enable Chrome remote debugging internal: "
          << should_enable_chrome_remote_debugging_
          << " "
          << is_chrome_remote_debugging_enabled_;
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

}  // namespace debugd
