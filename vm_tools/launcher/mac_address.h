// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_LAUNCHER_MAC_ADDRESS_H_
#define VM_TOOLS_LAUNCHER_MAC_ADDRESS_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "vm_tools/launcher/pooled_resource.h"

namespace vm_tools {
namespace launcher {

// Generates locally managed EUI-48 MAC addresses and ensures no collisions.
class MacAddress : public PooledResource {
 public:
  using Octets = std::array<uint8_t, 6>;

  ~MacAddress() override;

  std::string ToString() const;

  static std::unique_ptr<MacAddress> Create(
      const base::FilePath& instance_runtime_dir);
  static std::unique_ptr<MacAddress> Load(
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
  MacAddress(const base::FilePath& instance_runtime_dir,
             bool release_on_destruction);

  bool IsValidMac(const Octets& candidate) const;
  bool IsMacAllocated(const Octets& candidate) const;

  std::vector<Octets> allocated_macs_;
  Octets selected_mac_;

  DISALLOW_COPY_AND_ASSIGN(MacAddress);
};
}  // namespace launcher
}  // namespace vm_tools

#endif  // VM_TOOLS_LAUNCHER_MAC_ADDRESS_H_
