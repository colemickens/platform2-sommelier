// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_SERVICE_H_
#define VM_TOOLS_CONCIERGE_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <arc/network/address_manager.h>
#include <arc/network/mac_address_generator.h>
#include <arc/network/subnet.h>
#include <base/callback.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/sequence_checker.h>
#include <base/synchronization/lock.h>
#include <base/threading/thread.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <grpcpp/grpcpp.h>

#include "vm_tools/concierge/power_manager_client.h"
#include "vm_tools/concierge/shill_client.h"
#include "vm_tools/concierge/startup_listener_impl.h"
#include "vm_tools/concierge/termina_vm.h"
#include "vm_tools/concierge/vm_interface.h"
#include "vm_tools/concierge/vsock_cid_pool.h"

namespace vm_tools {

namespace concierge {

class SyncVmTimesResponse;

// VM Launcher Service responsible for responding to DBus method calls for
// starting, stopping, and otherwise managing VMs.
class Service final : public base::MessageLoopForIO::Watcher {
 public:
  // Creates a new Service instance.  |quit_closure| is posted to the TaskRunner
  // for the current thread when this process receives a SIGTERM.
  static std::unique_ptr<Service> Create(base::Closure quit_closure);
  ~Service() override;

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  explicit Service(base::Closure quit_closure);

  // Initializes the service by connecting to the system DBus daemon, exporting
  // its methods, and taking ownership of it's name.
  bool Init();

  // Handles the termination of a child process.
  void HandleChildExit();

  // Handles a SIGTERM.
  void HandleSigterm();

  // Handles a request to start a VM.  |method_call| must have a StartVmRequest
  // protobuf serialized as an array of bytes.
  std::unique_ptr<dbus::Response> StartVm(dbus::MethodCall* method_call);

  // Handles a request to start a plugin-based VM.  |method_call| must have a
  // StartPluginVmRequest protobuf serialized as an array of bytes.
  std::unique_ptr<dbus::Response> StartPluginVm(dbus::MethodCall* method_call);

  // Handles a request to stop a VM.  |method_call| must have a StopVmRequest
  // protobuf serialized as an array of bytes.
  std::unique_ptr<dbus::Response> StopVm(dbus::MethodCall* method_call);

  // Handles a request to stop all running VMs.
  std::unique_ptr<dbus::Response> StopAllVms(dbus::MethodCall* method_call);

  // Handles a request to get VM info.
  std::unique_ptr<dbus::Response> GetVmInfo(dbus::MethodCall* method_call);

  // Handles a request to update all VMs' times to the current host time.
  std::unique_ptr<dbus::Response> SyncVmTimes(dbus::MethodCall* method_call);

  // Business logic for SyncVmTimes that can be called without dbus.
  SyncVmTimesResponse SyncVmTimesInternal();

  // Handles a request to create a disk image.
  std::unique_ptr<dbus::Response> CreateDiskImage(
      dbus::MethodCall* method_call);

  // Handles a request to destroy a disk image.
  std::unique_ptr<dbus::Response> DestroyDiskImage(
      dbus::MethodCall* method_call);

  // Handles a request to export a disk image.
  std::unique_ptr<dbus::Response> ExportDiskImage(
      dbus::MethodCall* method_call);

  // Handles a request to list existing disk images.
  std::unique_ptr<dbus::Response> ListVmDisks(dbus::MethodCall* method_call);

  // Handles a request to get the SSH keys for a container.
  std::unique_ptr<dbus::Response> GetContainerSshKeys(
      dbus::MethodCall* method_call);

  std::unique_ptr<dbus::Response> AttachUsbDevice(
      dbus::MethodCall* method_call);

  std::unique_ptr<dbus::Response> DetachUsbDevice(
      dbus::MethodCall* method_call);

  std::unique_ptr<dbus::Response> ListUsbDevices(dbus::MethodCall* method_call);

  std::unique_ptr<dbus::Response> GetDnsSettings(dbus::MethodCall* method_call);

  // Writes DnsConfigResponse protobuf into DBus message.
  void ComposeDnsResponse(dbus::MessageWriter* writer);

  // Handles DNS changes from shill.
  void OnResolvConfigChanged(std::vector<std::string> nameservers,
                             std::vector<std::string> search_domains);

  // Helper for starting termina VMs, e.g. starting lxd.
  bool StartTermina(TerminaVm* vm, std::string* failure_reason);

  // Helpers for notifying cicerone of VM started/stopped events and generating
  // container tokens.
  void NotifyCiceroneOfVmStarted(const std::string& owner_id,
                                 const std::string& vm_name,
                                 uint32_t vsock_cid,
                                 std::string vm_token);
  void NotifyCiceroneOfVmStopped(const std::string& owner_id,
                                 const std::string& vm_name);
  std::string GetContainerToken(const std::string& owner_id,
                                const std::string& vm_name,
                                const std::string& container_name);

  void OnTremplinStartedSignal(dbus::Signal* signal);

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected);

  // Called by |power_manager_client_| when the device is about to suspend or
  // resumed from suspend.
  void HandleSuspendImminent();
  void HandleSuspendDone();

  using VmMap = std::map<std::pair<std::string, std::string>,
                         std::unique_ptr<VmInterface>>;

  // Returns an iterator to vm with key (|owner_id|, |vm_name|). If no such
  // element exists, tries the former with |owner_id| equal to empty string.
  VmMap::iterator FindVm(std::string owner_id, std::string vm_name);

  // Resource allocators for VMs.
  arc_networkd::MacAddressGenerator mac_address_generator_;
  arc_networkd::AddressManager network_address_manager_;
  VsockCidPool vsock_cid_pool_;

  // Current DNS resolution config.
  std::vector<std::string> nameservers_;
  std::vector<std::string> search_domains_;

  // File descriptor for the SIGCHLD events.
  base::ScopedFD signal_fd_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  // Connection to the system bus.
  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;       // Owned by |bus_|.
  dbus::ObjectProxy* cicerone_service_proxy_;   // Owned by |bus_|.
  dbus::ObjectProxy* seneschal_service_proxy_;  // Owned by |bus_|.

  // The port number to assign to the next shared directory server.
  uint32_t next_seneschal_server_port_;

  // The subnet for plugin VMs.
  std::unique_ptr<arc_networkd::Subnet> plugin_subnet_;
  std::unique_ptr<arc_networkd::SubnetAddress> plugin_gateway_;

  // Active VMs keyed by (owner_id, vm_name).
  VmMap vms_;

  // Active termina VMs.  Must come after |vms_| so that we don't end up with
  // dangling pointers.
  std::vector<TerminaVm*> termina_vms_;

  // The shill D-Bus client.
  std::unique_ptr<ShillClient> shill_client_;

  // The power manager D-Bus client.
  std::unique_ptr<PowerManagerClient> power_manager_client_;

  // The StartupListener service.
  StartupListenerImpl startup_listener_;

  // Thread on which the StartupListener service lives.
  base::Thread grpc_thread_vm_{"gRPC VM Server Thread"};

  // The server where the StartupListener service lives.
  std::shared_ptr<grpc::Server> grpc_server_vm_;

  // Closure that's posted to the current thread's TaskRunner when the service
  // receives a SIGTERM.
  base::Closure quit_closure_;

  // Ensure calls are made on the right thread.
  base::SequenceChecker sequence_checker_;

  // Signal must be connected before we can call SetTremplinStarted in a VM.
  bool is_tremplin_started_signal_connected_ = false;

  // Indicates that the VMs are currently suspended and may not respond to RPCs.
  bool vms_suspended_ = false;

  // Indicates that we should update the resolv.conf file in each VM after
  // resume.  This can happen if we get a ResolvCongigChanged message from shill
  // before receiving a SuspendDone signal from powerd.  Attempting to update
  // the resolv.conf file will simply result in a timeout so we should do it
  // after a resume.
  bool update_resolv_config_on_resume_ = false;

  // Whether we should re-synchronize VM clocks on resume from sleep.
  const bool resync_vm_clocks_on_resume_;

  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_SERVICE_H_
