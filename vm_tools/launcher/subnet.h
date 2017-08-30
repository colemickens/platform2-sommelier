// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_LAUNCHER_SUBNET_H_
#define VM_TOOLS_LAUNCHER_SUBNET_H_

#include <bitset>
#include <memory>
#include <string>

#include <base/macros.h>

#include "vm_tools/launcher/pooled_resource.h"

namespace vm_tools {
namespace launcher {

// Manages IPv4 subnets that can be assigned to VMs.
class Subnet : public PooledResource {
 public:
  Subnet() = default;
  virtual ~Subnet();

  std::string GetGatewayAddress() const;
  std::string GetIpAddress() const;
  std::string GetNetmask() const;

  static std::unique_ptr<Subnet> Create();

 protected:
  // PooledResource overrides
  const char* GetName() const override;
  bool LoadResources(const std::string& resources) override;
  std::string PersistResources() override;
  bool AllocateResource() override;
  bool ReleaseResource() override;

 private:
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
