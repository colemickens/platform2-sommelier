// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_ADAPTER_INTERFACE_HANDLER_H_
#define BLUETOOTH_NEWBLUED_ADAPTER_INTERFACE_HANDLER_H_

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <brillo/errors/error.h>
#include <dbus/bus.h>
#include <dbus/message.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/newblue.h"

namespace bluetooth {

// Handles org.bluez.Adapter1 interface.
class AdapterInterfaceHandler {
 public:
  // |newblue| and |exported_object_manager_wrapper| not owned, caller must make
  // sure it outlives this object.
  AdapterInterfaceHandler(
      scoped_refptr<dbus::Bus> bus,
      Newblue* newblue,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper);
  virtual ~AdapterInterfaceHandler() = default;

  // Starts exposing org.bluez.Adapter1 interface on object /org/bluez/hci0.
  // The properties of this object will be ignored by btdispatch, but the object
  // still has to be exposed to be able to receive org.bluez.Adapter1 method
  // calls, e.g. StartDiscovery(), StopDiscovery().
  void Init(Newblue::DeviceDiscoveredCallback device_discovered_callback);

 private:
  // D-Bus method handlers for adapter object.
  bool HandleStartDiscovery(brillo::ErrorPtr* error, dbus::Message* message);
  bool HandleStopDiscovery(brillo::ErrorPtr* error, dbus::Message* message);

  scoped_refptr<dbus::Bus> bus_;

  Newblue* newblue_;

  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;

  Newblue::DeviceDiscoveredCallback device_discovered_callback_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<AdapterInterfaceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AdapterInterfaceHandler);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_ADAPTER_INTERFACE_HANDLER_H_
