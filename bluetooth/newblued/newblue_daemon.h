// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_
#define BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_

#include <map>
#include <memory>
#include <string>

#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>

#include "bluetooth/common/bluetooth_daemon.h"
#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/adapter_interface_handler.h"
#include "bluetooth/newblued/advertising_manager_interface_handler.h"
#include "bluetooth/newblued/agent_manager_interface_handler.h"
#include "bluetooth/newblued/device_interface_handler.h"
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/stack_sync_monitor.h"

namespace bluetooth {

class NewblueDaemon : public BluetoothDaemon {
 public:
  NewblueDaemon(std::unique_ptr<Newblue> newblue, bool is_idle_mode);
  ~NewblueDaemon() override = default;

  // BluetoothDaemon override:
  bool Init(scoped_refptr<dbus::Bus> bus, DBusDaemon* dbus_daemon) override;

  // Frees up all resources. Currently only needed in test.
  void Shutdown();

  // Called when NewBlue is ready to be brought up.
  void OnHciReadyForUp();

 private:
  // Registers GetAll/Get/Set method handlers.
  void SetupPropertyMethodHandlers(
      brillo::dbus_utils::DBusInterface* prop_interface,
      brillo::dbus_utils::ExportedPropertySet* property_set);

  void OnBluezDown();

  // True means that newblued does not do anything but only keeps itself alive.
  // This mode is needed as LE kernel splitter is not (yet) enabled for all
  // boards, so newblued should be in idle mode for those boards without LE
  // kernel splitter.
  bool is_idle_mode_;

  scoped_refptr<dbus::Bus> bus_;

  std::unique_ptr<ExportedObjectManagerWrapper>
      exported_object_manager_wrapper_;

  std::unique_ptr<Newblue> newblue_;

  DBusDaemon* dbus_daemon_;

  StackSyncMonitor stack_sync_monitor_;

  std::unique_ptr<AdapterInterfaceHandler> adapter_interface_handler_;
  std::unique_ptr<DeviceInterfaceHandler> device_interface_handler_;
  std::unique_ptr<AdvertisingManagerInterfaceHandler>
      advertising_manager_interface_handler_;
  std::unique_ptr<AgentManagerInterfaceHandler>
      agent_manager_interface_handler_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<NewblueDaemon> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NewblueDaemon);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_
