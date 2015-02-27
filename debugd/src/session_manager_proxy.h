// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_SESSION_MANAGER_PROXY_H_
#define DEBUGD_SRC_SESSION_MANAGER_PROXY_H_

#include <string>

#include <base/macros.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>
#include <session_manager/dbus_proxies/org.chromium.SessionManagerInterface.h>

namespace debugd {

// Talks to Session Manager DBus interface.  It also exposes
// convenience method to enable Chrome remote debugging and listens to Session
// Manager signal to ensure Chrome remote debugging is on when it is supposed
// to be.
class SessionManagerProxy : public org::chromium::SessionManagerInterface_proxy,
                            public DBus::ObjectProxy {
 public:
  explicit SessionManagerProxy(DBus::Connection* connection)
      : DBus::ObjectProxy(*connection,
                          login_manager::kSessionManagerServicePath,
                          login_manager::kSessionManagerServiceName) {}
  ~SessionManagerProxy() override = default;
  void LoginPromptVisible() override;
  void SessionStateChanged(const std::string &state,
                           const std::string &user) override {};
  void ScreenIsLocked() override {};
  void ScreenIsUnlocked() override {};

  // Sets up the proxy for Chrome remote debugging and tries to enable it.
  void EnableChromeRemoteDebugging();

 private:
  // Tries to enable Chrome remote debugging.
  void EnableChromeRemoteDebuggingInternal();

  // Should the proxy try to enable Chrome remote debugging.
  bool should_enable_chrome_remote_debugging_ = false;
  // Whether Chrome remote debugging is already successfully enabled.
  bool is_chrome_remote_debugging_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerProxy);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_SESSION_MANAGER_PROXY_H_
