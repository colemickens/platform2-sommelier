// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_LAUNCHER_MAC_ADDRESS_H_
#define VM_LAUNCHER_MAC_ADDRESS_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "vm_launcher/pooled_resource.h"

namespace vm_launcher {

// Generates locally managed EUI-48 MAC addresses and ensures no collisions.
class MacAddress : public PooledResource {
 public:
  using Octets = std::array<uint8_t, 6>;

  MacAddress() = default;
  virtual ~MacAddress();

  std::string ToString() const;

  static std::unique_ptr<MacAddress> Create();

 protected:
  // PooledResource overrides
  const char* GetName() const override;
  bool LoadResources(const std::string& resources) override;
  std::string PersistResources() override;
  bool AllocateResource() override;
  bool ReleaseResource() override;

 private:
  bool IsValidMac(const Octets& candidate) const;

  std::vector<Octets> allocated_macs_;
  Octets selected_mac_;

  DISALLOW_COPY_AND_ASSIGN(MacAddress);
};
}  // namespace vm_launcher

#endif  // VM_LAUNCHER_MAC_ADDRESS_H_
