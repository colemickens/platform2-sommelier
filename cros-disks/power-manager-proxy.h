// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_POWER_MANAGER_PROXY_H_
#define CROS_DISKS_POWER_MANAGER_PROXY_H_

#include <dbus-c++/dbus.h>  // NOLINT

#include <base/basictypes.h>

namespace cros_disks {

class PowerManagerObserver;

// A proxy class that listens to DBus signals from the power manager and
// notifies a registered observer for events.
class PowerManagerProxy : public DBus::InterfaceProxy,
                          public DBus::ObjectProxy {
 public:
  PowerManagerProxy(DBus::Connection* connection,
      PowerManagerObserver* observer);

  ~PowerManagerProxy();

 private:
  // Handles the ScreenIsLocked DBus signal.
  void OnScreenIsLocked(const DBus::SignalMessage& signal);

  // Handles the ScreenIsUnlocked DBus signal.
  void OnScreenIsUnlocked(const DBus::SignalMessage& signal);

  PowerManagerObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerProxy);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_POWER_MANAGER_PROXY_H_
