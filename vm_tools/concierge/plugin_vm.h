// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_PLUGIN_VM_H_
#define VM_TOOLS_CONCIERGE_PLUGIN_VM_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <arc/network/mac_address_generator.h>
#include <arc/network/subnet.h>
#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <brillo/process.h>

#include "vm_tools/concierge/vm_interface.h"

namespace vm_tools {
namespace concierge {

class PluginVm final : public VmInterface {
 public:
  static std::unique_ptr<PluginVm> Create(
      uint32_t cpus,
      std::string params,
      arc_networkd::MacAddress mac_addr,
      std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
      uint32_t ipv4_netmask,
      uint32_t ipv4_gateway,
      base::FilePath stateful_dir,
      base::FilePath runtime_dir,
      base::FilePath cicerone_token_dir);
  ~PluginVm() override;

  // VmInterface overrides.
  bool Shutdown() override;
  VmInterface::Info GetInfo() override;
  bool AttachUsbDevice(uint8_t bus,
                       uint8_t addr,
                       uint16_t vid,
                       uint16_t pid,
                       int fd,
                       UsbControlResponse* response) override;
  bool DetachUsbDevice(uint8_t port, UsbControlResponse* response) override;
  bool ListUsbDevice(std::vector<UsbDevice>* devices) override;
  void HandleSuspendImminent() override {}
  void HandleSuspendDone() override {}

 private:
  PluginVm(arc_networkd::MacAddress mac_addr,
           std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
           uint32_t ipv4_netmask,
           uint32_t ipv4_gateway,
           base::FilePath runtime_dir);
  bool Start(uint32_t cpus,
             std::string params,
             base::FilePath stateful_dir,
             base::FilePath cicerone_token_dir);

  // Runtime directory for the crosvm instance.
  base::ScopedTempDir runtime_dir_;

  // Handle to the VM process.
  brillo::ProcessImpl process_;

  // Network configuration.
  arc_networkd::MacAddress mac_addr_;
  std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr_;
  uint32_t netmask_;
  uint32_t gateway_;

  DISALLOW_COPY_AND_ASSIGN(PluginVm);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_PLUGIN_VM_H_
