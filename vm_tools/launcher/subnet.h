// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_LAUNCHER_SUBNET_H_
#define VM_TOOLS_LAUNCHER_SUBNET_H_

#include <bitset>
#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "vm_tools/launcher/pooled_resource.h"

namespace vm_tools {
namespace launcher {

// Manages IPv4 subnets that can be assigned to VMs.
class Subnet : public PooledResource {
 public:
  ~Subnet() override;

  std::string GetGatewayAddress() const;
  std::string GetIpAddress() const;
  std::string GetNetmask() const;

  static std::shared_ptr<Subnet> Create(
      const base::FilePath& instance_runtime_dir);
  static std::shared_ptr<Subnet> Load(
      const base::FilePath& instance_runtime_dir);

 protected:
  // PooledResource overrides
  const char* GetName() const override;
  const std::string GetResourceID() const override;
  bool LoadGlobalResources(const std::string& resources) override;
  std::string PersistGlobalResources() override;
  bool LoadInstanceResource(const std::string& resource) override;
  bool AllocateResource() override;
  bool ReleaseResource() override;

 private:
  Subnet(const base::FilePath& instance_runtime_dir,
         bool release_on_destruction);

  bool IsSubnetAllocated(const size_t subnet_id) const;
  // For simplicity, divide our /24 into 64 /30 subnets. Each subnet can then
  // be referred to by an id from 0-63. Within each subnet:
  // addr 0 - network identifier
  // addr 1 - gateway (host) address
  // addr 2 - VM (guest) address
  // addr 3 - broadcast address
  std::bitset<64> allocated_subnets_;
  size_t selected_subnet_;

  DISALLOW_COPY_AND_ASSIGN(Subnet);
};
}  // namespace launcher
}  // namespace vm_tools

#endif  // VM_TOOLS_LAUNCHER_SUBNET_H_
