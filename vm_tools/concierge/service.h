// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_SERVICE_H_
#define VM_TOOLS_CONCIERGE_SERVICE_H_

#include <memory>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

namespace vm_tools {
namespace concierge {

// VM Launcher Service responsible for responding to DBus method calls for
// starting, stopping, and otherwise managing VMs.
class Service {
 public:
  static std::unique_ptr<Service> Create();
  ~Service() = default;

 private:
  Service() = default;

  // Initializes the service by connecting to the system DBus daemon, exporting
  // its methods, and taking ownership of it's name.
  bool Init();

  // Handles a request to start a VM.  |method_call| must have a StartVmRequest
  // protobuf serialized as an array of bytes.
  std::unique_ptr<dbus::Response> StartVm(dbus::MethodCall* method_call);

  // Handles a request to stop a VM.  |method_call| must have a StopVmRequest
  // protobuf serialized as an array of bytes.
  std::unique_ptr<dbus::Response> StopVm(dbus::MethodCall* method_call);

  // Handles a request to stop all running VMs.
  std::unique_ptr<dbus::Response> StopAllVms(dbus::MethodCall* method_call);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;  // Owned by |bus_|.

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_SERVICE_H_
