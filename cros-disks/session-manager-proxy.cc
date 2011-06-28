// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/session-manager-proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "cros-disks/session-manager-observer.h"

namespace cros_disks {

SessionManagerProxy::SessionManagerProxy(DBus::Connection* connection,
                                         SessionManagerObserver* observer)
    : DBus::InterfaceProxy(login_manager::kSessionManagerInterface),
      DBus::ObjectProxy(*connection,
                        login_manager::kSessionManagerServicePath,
                        login_manager::kSessionManagerServiceName),
      observer_(observer) {
  CHECK(observer_) << "Invalid session manager observer";
  connect_signal(SessionManagerProxy, SessionStateChanged,
                 OnSessionStateChanged);
}

SessionManagerProxy::~SessionManagerProxy() {
}

void SessionManagerProxy::OnSessionStateChanged(
    const DBus::SignalMessage& signal) {
  DBus::MessageIter reader = signal.reader();
  std::string state, user;
  reader >> state >> user;
  if (state == "started") {
    observer_->OnSessionStarted(user);
  } else if (state == "stopped") {
    observer_->OnSessionStopped(user);
  }
}

}  // namespace cros_disks
