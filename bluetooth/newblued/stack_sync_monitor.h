// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_STACK_SYNC_MONITOR_H_
#define BLUETOOTH_NEWBLUED_STACK_SYNC_MONITOR_H_

#include <string>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <dbus/property.h>
#include <gtest/gtest_prod.h>

namespace bluetooth {

class StackSyncMonitor : public dbus::ObjectManager::Interface {
 public:
  StackSyncMonitor();

  // |bus| owned by client and not kept after this method returns.
  void RegisterBluezDownCallback(dbus::Bus* bus, base::Closure callback);

  // dbus::ObjectManager::Interface overrides.
  dbus::PropertySet* CreateProperties(dbus::ObjectProxy* object_proxy,
                                      const dbus::ObjectPath& object_path,
                                      const std::string& interface) override;
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface) override;

 private:
  FRIEND_TEST(StackSyncMonitorTest, PowerDown);
  FRIEND_TEST(StackSyncMonitorTest, BluezStackSyncQuitting);
  FRIEND_TEST(StackSyncMonitorTest, NoCallback);

  void OnBluezPropertyChanged(const std::string& name);

  base::Closure callback_;
  bool cached_bluez_powered_;
  dbus::Property<bool> bluez_powered_;
  dbus::Property<bool> bluez_stack_sync_quitting_;
  base::WeakPtrFactory<StackSyncMonitor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StackSyncMonitor);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_STACK_SYNC_MONITOR_H_
