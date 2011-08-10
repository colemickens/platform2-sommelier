// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_POWER_MANAGER_PROXY_H_
#define CROS_DISKS_POWER_MANAGER_PROXY_H_

#include <dbus-c++/dbus.h>  // NOLINT

#include <base/basictypes.h>
#include <base/observer_list.h>

namespace cros_disks {

class PowerManagerObserverInterface;

// A proxy class that listens to DBus signals from the power manager and
// notifies a list of registered observers for events.
class PowerManagerProxy : public DBus::InterfaceProxy,
                          public DBus::ObjectProxy {
 public:
  explicit PowerManagerProxy(DBus::Connection* connection);

  ~PowerManagerProxy();

  void AddObserver(PowerManagerObserverInterface* observer);

 private:
  // Handles the ScreenIsLocked DBus signal.
  void OnScreenIsLocked(const DBus::SignalMessage& signal);

  // Handles the ScreenIsUnlocked DBus signal.
  void OnScreenIsUnlocked(const DBus::SignalMessage& signal);

  ObserverList<PowerManagerObserverInterface> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerProxy);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_POWER_MANAGER_PROXY_H_
