// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SESSION_MANAGER_PROXY_H_
#define CROS_DISKS_SESSION_MANAGER_PROXY_H_

#include <dbus-c++/dbus.h>  // NOLINT

#include <base/basictypes.h>
#include <base/observer_list.h>

namespace cros_disks {

class SessionManagerObserverInterface;

// A proxy class that listens to DBus signals from the session manager and
// notifies a list of registered observers for events.
class SessionManagerProxy : public DBus::InterfaceProxy,
                            public DBus::ObjectProxy {
 public:
  explicit SessionManagerProxy(DBus::Connection* connection);

  ~SessionManagerProxy();

  void AddObserver(SessionManagerObserverInterface* observer);

 private:
  // Handles the ScreenIsLocked DBus signal.
  void OnScreenIsLocked(const DBus::SignalMessage& signal);

  // Handles the ScreenIsUnlocked DBus signal.
  void OnScreenIsUnlocked(const DBus::SignalMessage& signal);

  // Handles the SessionStateChanged DBus signal.
  void OnSessionStateChanged(const DBus::SignalMessage& signal);

  ObserverList<SessionManagerObserverInterface> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerProxy);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SESSION_MANAGER_PROXY_H_
