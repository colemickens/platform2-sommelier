// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_DEBUG_MANAGER_H_
#define BLUETOOTH_NEWBLUED_DEBUG_MANAGER_H_

#include <string>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <dbus/property.h>

namespace bluetooth {

// This class subscribes the NewblueLevel property of D-Bus interface
// org.chromium.Bluetooth.Debug, and use it to set the verbosity level of
// newblue daemon.
class DebugManager final : public dbus::ObjectManager::Interface {
 public:
  explicit DebugManager(scoped_refptr<dbus::Bus> bus);
  ~DebugManager() override = default;

  void Init();

  // dbus::ObjectManager::Interface overrides.
  dbus::PropertySet* CreateProperties(dbus::ObjectProxy* object_proxy,
                                      const dbus::ObjectPath& object_path,
                                      const std::string& interface) override;

 private:
  void OnPropertyChanged(const std::string& prop_name);

  void SetNewblueLogLevel(int verbosity);

  scoped_refptr<dbus::Bus> bus_;

  dbus::Property<uint8_t> newblue_level_;

  int current_verbosity_ = 0;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<DebugManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DebugManager);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_DEBUG_MANAGER_H_
