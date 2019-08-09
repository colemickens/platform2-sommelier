// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_ARC_VM_H_
#define VM_TOOLS_CONCIERGE_ARC_VM_H_

#include <stdint.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <arc/network/mac_address_generator.h>
#include <arc/network/subnet.h>
#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <brillo/process.h>
#include <vm_concierge/proto_bindings/service.pb.h>

#include "vm_tools/concierge/seneschal_server_proxy.h"
#include "vm_tools/concierge/vm_interface.h"
#include "vm_tools/concierge/vsock_cid_pool.h"

namespace vm_tools {
namespace concierge {

struct ArcVmFeatures {
  // Enable GPU in the started VM.
  bool gpu;
};

// Represents a single instance of a running termina VM.
class ArcVm final : public VmInterface {
 public:
  // Describes a disk image to be mounted inside the VM.
  struct Disk {
    // Path to the disk image on the host.
    base::FilePath path;

    // Whether the disk should be writable by the VM.
    bool writable;
  };

  // Starts a new virtual machine.  Returns nullptr if the virtual machine
  // failed to start for any reason.
  static std::unique_ptr<ArcVm> Create(
      base::FilePath kernel,
      base::FilePath rootfs,
      std::vector<Disk> disks,
      arc_networkd::MacAddress mac_addr,
      std::unique_ptr<arc_networkd::Subnet> subnet,
      uint32_t vsock_cid,
      std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
      base::FilePath runtime_dir,
      ArcVmFeatures features,
      std::vector<std::string> params);
  ~ArcVm() override;

  // The pid of the child process.
  pid_t pid() { return process_.pid(); }

  // The VM's cid.
  uint32_t cid() const { return vsock_cid_; }

  bool enable_gpu() const { return features_.gpu; }

  // The 9p server managed by seneschal that provides access to shared files for
  // this VM.  Returns 0 if there is no seneschal server associated with this
  // VM.
  uint32_t seneschal_server_handle() const {
    return seneschal_server_proxy_ ? seneschal_server_proxy_->handle() : 0;
  }

  // The IPv4 address of the VM's gateway in network byte order.
  uint32_t GatewayAddress() const;

  // The IPv4 address of the VM in network byte order.
  uint32_t IPv4Address() const;

  // The netmask of the VM's subnet in network byte order.
  uint32_t Netmask() const;

  // VmInterface overrides.
  bool Shutdown() override;
  VmInterface::Info GetInfo() override;
  // Currently only implemented for termina, returns "Not implemented".
  bool GetVmEnterpriseReportingInfo(
      GetVmEnterpriseReportingInfoResponse* response) override;
  bool AttachUsbDevice(uint8_t bus,
                       uint8_t addr,
                       uint16_t vid,
                       uint16_t pid,
                       int fd,
                       UsbControlResponse* response) override;
  bool DetachUsbDevice(uint8_t port, UsbControlResponse* response) override;
  bool ListUsbDevice(std::vector<UsbDevice>* devices) override;
  void HandleSuspendImminent() override;
  void HandleSuspendDone() override;
  bool SetResolvConfig(
      const std::vector<std::string>& nameservers,
      const std::vector<std::string>& search_domains) override {
    return true;
  }
  // TODO(b/136143058): Implement SetTime calls.
  bool SetTime(std::string* failure_reason) override { return true; }
  void SetTremplinStarted() override { NOTREACHED(); }

  // Adjusts the amount of CPU the ARCVM processes are allowed to use.
  static bool SetVmCpuRestriction(CpuRestrictionState cpu_restriction_state);

 private:
  ArcVm(arc_networkd::MacAddress mac_addr,
        std::unique_ptr<arc_networkd::Subnet> subnet,
        uint32_t vsock_cid,
        std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
        base::FilePath runtime_dir,
        ArcVmFeatures features);

  // Returns the path to the VM control socket.
  std::string GetVmSocketPath() const;

  // Starts the VM with the given kernel and root file system.
  bool Start(base::FilePath kernel,
             base::FilePath rootfs,
             std::vector<Disk> disks,
             std::vector<std::string> params);

  // EUI-48 mac address for the VM's network interface.
  arc_networkd::MacAddress mac_addr_;

  // The /30 subnet assigned to the VM.
  std::unique_ptr<arc_networkd::Subnet> subnet_;

  // Virtual socket context id to be used when communicating with this VM.
  uint32_t vsock_cid_;

  // Proxy to the server providing shared directory access for this VM.
  std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy_;

  // Runtime directory for this VM.
  base::ScopedTempDir runtime_dir_;

  // Flags passed to vmc start.
  ArcVmFeatures features_;

  // Handle to the VM process.
  brillo::ProcessImpl process_;

  DISALLOW_COPY_AND_ASSIGN(ArcVm);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_ARC_VM_H_
