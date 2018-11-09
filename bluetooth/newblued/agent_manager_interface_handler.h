// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_AGENT_MANAGER_INTERFACE_HANDLER_H_
#define BLUETOOTH_NEWBLUED_AGENT_MANAGER_INTERFACE_HANDLER_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/message.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/newblue.h"

namespace bluetooth {

// Handles org.bluez.AgentManager1 interface.
class AgentManagerInterfaceHandler : public PairingAgent {
 public:
  // |exported_object_manager_wrapper| not owned, caller must make sure it
  // outlives this object.
  AgentManagerInterfaceHandler(
      scoped_refptr<dbus::Bus> bus,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper);
  virtual ~AgentManagerInterfaceHandler() = default;

  // Starts exposing org.bluez.AgentManager1 interface.
  void Init();

  // PairingAgent overrides.
  void DisplayPasskey(const std::string& device_address,
                      uint32_t passkey) override;

 private:
  scoped_refptr<dbus::Bus> bus_;

  // Client D-Bus address -> Agent object path.
  std::map<std::string, dbus::ObjectPath> agent_object_paths_;
  // The default agent, should be one of the keys of |agent_object_paths_|.
  std::string default_agent_client_;

  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;

  void OnDisplayPasskeySent(dbus::Response* response);

  // D-Bus method handlers for agent manager.
  // Right now we only support 1 agent path per client, since that's how Chrome
  // and bluetoothctl uses this.
  // TODO(sonnysasaka): Add support for multiple agent path per client when
  // needed.
  bool HandleRegisterAgent(brillo::ErrorPtr* error,
                           dbus::Message* message,
                           dbus::ObjectPath agent_object_path,
                           std::string capability);
  bool HandleUnregisterAgent(brillo::ErrorPtr* error,
                             dbus::Message* message,
                             dbus::ObjectPath agent_object_path);
  bool HandleRequestDefaultAgent(brillo::ErrorPtr* error,
                                 dbus::Message* message,
                                 dbus::ObjectPath agent_object_path);

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<AgentManagerInterfaceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AgentManagerInterfaceHandler);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_AGENT_MANAGER_INTERFACE_HANDLER_H_
