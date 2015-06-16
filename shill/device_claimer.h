// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_CLAIMER_H_
#define SHILL_DEVICE_CLAIMER_H_

#include <set>
#include <string>

#include "shill/dbus_manager.h"
#include "shill/error.h"

namespace shill {

class DeviceInfo;

// Provide an abstraction for remote service to claim/release devices
// from/to shill. When the DBus service name is provided, which means that
// the remote service is a DBus endpoint, Shill will perform DBus monitoring
// on that service, and revert all operations performed by that service when
// it disappears.
class DeviceClaimer {
 public:
  DeviceClaimer(
      const std::string& dbus_service_name,
      DeviceInfo* device_info,
      bool default_claimer);
  virtual ~DeviceClaimer();

  virtual bool StartDBusNameWatcher(
      DBusManager* dbus_manager,
      const DBusNameWatcher::NameAppearedCallback& name_appeared_callback,
      const DBusNameWatcher::NameVanishedCallback& name_vanished_callback);

  virtual bool Claim(const std::string& device_name, Error* error);
  virtual bool Release(const std::string& device_name, Error* error);

  // Return true if there are devices claimed by this claimer, false
  // otherwise.
  virtual bool DevicesClaimed();

  // Return true if the specified device is released by this claimer, false
  // otherwise.
  virtual bool IsDeviceReleased(const std::string& device_name);

  const std::string& name() const { return dbus_service_name_; }
  const std::string& owner() const { return dbus_service_owner_; }
  void set_owner(const std::string& dbus_service_owner) {
    dbus_service_owner_ = dbus_service_owner;
  }

  virtual bool default_claimer() const { return default_claimer_; }

  const std::set<std::string>& claimed_device_names() const {
    return claimed_device_names_;
  }

 private:
  // DBus name watcher for monitoring the DBus service of the claimer.
  std::unique_ptr<DBusNameWatcher> dbus_name_watcher_;
  // The name of devices that have been claimed by this claimer.
  std::set<std::string> claimed_device_names_;
  // The name of devices that have been released by this claimer.
  std::set<std::string> released_device_names_;
  // DBus service name of the claimer.
  std::string dbus_service_name_;
  // DBus service owner name of the claimer.
  std::string dbus_service_owner_;

  DeviceInfo* device_info_;

  // Flag indicating if this is the default claimer. When set to true, this
  // claimer will only be deleted when shill terminates.
  bool default_claimer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceClaimer);
};

}  // namespace shill

#endif  // SHILL_DEVICE_CLAIMER_H_
