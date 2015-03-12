// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_SESSION_MANAGER_PROXY_H_
#define DEBUGD_SRC_SESSION_MANAGER_PROXY_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus_proxies/org.freedesktop.DBus.h>
#include <dbus-1.0/dbus/dbus-shared.h>
#include <dbus-c++/dbus.h>
#include <session_manager/dbus_proxies/org.chromium.SessionManagerInterface.h>

namespace debugd {

class SessionManagerProxy;

namespace session_manager_internal {

// Talks to the system DBus interface.  This class is responsible for monitoring
// the dependent Session Manager DBus interface and retry enabling Chrome remote
// debugging when service becomes available.
class DBusProxy : public org::freedesktop::DBus_proxy,
                  public DBus::ObjectProxy {
 public:
  DBusProxy(DBus::Connection* connection, SessionManagerProxy* session_manager)
      : DBus::ObjectProxy(*connection, DBUS_PATH_DBUS, DBUS_SERVICE_DBUS),
        session_manager_(session_manager) {}
  ~DBusProxy() override = default;

  void NameOwnerChanged(const std::string& name,
                        const std::string& /*new_owner*/,
                        const std::string& /*old_owner*/) override;

  void NameLost(const std::string& /*name*/) override {};
  void NameAcquired(const std::string& /*name*/) override {};

 private:
  SessionManagerProxy* session_manager_;
};

}  // namespace session_manager_internal

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
  void LoginPromptVisible() override {};
  void SessionStateChanged(const std::string &state) override {};
  void ScreenIsLocked() override {};
  void ScreenIsUnlocked() override {};

  // Sets up the proxy for Chrome remote debugging and tries to enable it.
  void EnableChromeRemoteDebugging();

  // Run tasks that cannot be run during signal handling.
  void MaybeRunCallback();

 private:
  // Tries to enable Chrome remote debugging.
  void EnableChromeRemoteDebuggingInternal();

  // Should the proxy try to enable Chrome remote debugging.
  bool should_enable_chrome_remote_debugging_ = false;
  // Whether Chrome remote debugging is already successfully enabled.
  bool is_chrome_remote_debugging_enabled_ = false;
  // Whether SessionManager DBus interface is ready.
  bool is_session_manager_ready_ = false;
  // Max number of times we would try to enable Chrome remote debugging.
  int enable_chrome_remote_debugging_try_counter_ = 10;

  std::unique_ptr<session_manager_internal::DBusProxy> dbus_proxy_;

  friend session_manager_internal::DBusProxy;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerProxy);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_SESSION_MANAGER_PROXY_H_
