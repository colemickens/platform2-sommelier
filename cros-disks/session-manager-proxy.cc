// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/session-manager-proxy.h"

#include <string>

#include <chromeos/dbus/service_constants.h>

#include "cros-disks/session-manager-observer-interface.h"

using std::string;

namespace cros_disks {

SessionManagerProxy::SessionManagerProxy(DBus::Connection* connection)
    : DBus::InterfaceProxy(login_manager::kSessionManagerInterface),
      DBus::ObjectProxy(*connection,
                        login_manager::kSessionManagerServicePath,
                        login_manager::kSessionManagerServiceName) {
  connect_signal(SessionManagerProxy, ScreenIsLocked, OnScreenIsLocked);
  connect_signal(SessionManagerProxy, ScreenIsUnlocked, OnScreenIsUnlocked);
  connect_signal(SessionManagerProxy, SessionStateChanged,
                 OnSessionStateChanged);
}

SessionManagerProxy::~SessionManagerProxy() {
}

void SessionManagerProxy::AddObserver(
    SessionManagerObserverInterface* observer) {
  CHECK(observer) << "Invalid observer object";
  observer_list_.AddObserver(observer);
}

void SessionManagerProxy::OnScreenIsLocked(const DBus::SignalMessage& signal) {
  FOR_EACH_OBSERVER(SessionManagerObserverInterface, observer_list_,
                    OnScreenIsLocked());
}

void SessionManagerProxy::OnScreenIsUnlocked(
    const DBus::SignalMessage& signal) {
  FOR_EACH_OBSERVER(SessionManagerObserverInterface, observer_list_,
                    OnScreenIsUnlocked());
}

void SessionManagerProxy::OnSessionStateChanged(
    const DBus::SignalMessage& signal) {
  DBus::MessageIter reader = signal.reader();
  string state, user;
  reader >> state >> user;
  if (state == "started") {
    FOR_EACH_OBSERVER(SessionManagerObserverInterface, observer_list_,
                      OnSessionStarted(user));
  } else if (state == "stopped") {
    FOR_EACH_OBSERVER(SessionManagerObserverInterface, observer_list_,
                      OnSessionStopped(user));
  }
}

}  // namespace cros_disks
