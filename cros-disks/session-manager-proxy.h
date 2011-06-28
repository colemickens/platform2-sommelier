// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SESSION_MANAGER_PROXY_H_
#define CROS_DISKS_SESSION_MANAGER_PROXY_H_

#include <dbus-c++/dbus.h>  // NOLINT
#include <string>

#include <base/basictypes.h>

namespace cros_disks {

class SessionManagerObserver;

// A proxy class that listens to DBus signals from the session manager and
// notifies a registered observer for events.
class SessionManagerProxy : public DBus::InterfaceProxy,
                            public DBus::ObjectProxy {
 public:
  SessionManagerProxy(DBus::Connection* connection,
      SessionManagerObserver* observer);

  ~SessionManagerProxy();

 private:
  // Handles the SessionStateChanged DBus signal.
  void OnSessionStateChanged(const DBus::SignalMessage& signal);

  SessionManagerObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerProxy);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SESSION_MANAGER_PROXY_H_
