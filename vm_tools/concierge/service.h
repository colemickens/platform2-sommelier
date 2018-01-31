// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_SERVICE_H_
#define VM_TOOLS_CONCIERGE_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <base/threading/thread.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <grpc++/grpc++.h>

#include "vm_tools/concierge/mac_address_generator.h"
#include "vm_tools/concierge/startup_listener_impl.h"
#include "vm_tools/concierge/subnet_pool.h"
#include "vm_tools/concierge/virtual_machine.h"
#include "vm_tools/concierge/vsock_cid_pool.h"

namespace vm_tools {
namespace concierge {

// VM Launcher Service responsible for responding to DBus method calls for
// starting, stopping, and otherwise managing VMs.
class Service final : public base::MessageLoopForIO::Watcher {
 public:
  static std::unique_ptr<Service> Create();
  ~Service() override;

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  Service();

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

  // Handles a request to get VM info.
  std::unique_ptr<dbus::Response> GetVmInfo(dbus::MethodCall* method_call);

  // Handles a request to create a disk image.
  std::unique_ptr<dbus::Response> CreateDiskImage(
      dbus::MethodCall* method_call);

  // Helper for starting termina VMs, e.g. starting lxd.
  bool StartTermina(VirtualMachine* vm, std::string* failure_reason);

  // Resource allocators for VMs.
  MacAddressGenerator mac_address_generator_;
  SubnetPool subnet_pool_;
  VsockCidPool vsock_cid_pool_;

  // File descriptor for the SIGCHLD events.
  base::ScopedFD signal_fd_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  // Active VMs.
  std::map<std::string, std::unique_ptr<VirtualMachine>> vms_;

  // Connection to the system bus.
  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;  // Owned by |bus_|.

  // The StartupListener service.
  StartupListenerImpl startup_listener_;

  // Thread on with the StartupListener service lives.
  base::Thread grpc_thread_{"gRPC Server Thread"};

  // The server where the StartupListener service lives.
  std::shared_ptr<grpc::Server> grpc_server_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_SERVICE_H_
