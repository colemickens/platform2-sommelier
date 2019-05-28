// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_SUBNET_POOL_H_
#define ARC_NETWORK_SUBNET_POOL_H_

#include <stdint.h>

#include <bitset>
#include <memory>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <brillo/brillo_export.h>

#include "arc/network/subnet.h"

namespace arc_networkd {

// Manages up to 32 IPv4 subnets that can be assigned to guest interfaces.
// These use non-publicly routable addresses in the range 100.115.92.0/24.
class BRILLO_EXPORT SubnetPool {
 public:
  // Returns a new pool or nullptr if num_subnets exceeds 32.
  static std::unique_ptr<SubnetPool> New(uint32_t base_addr,
                                         uint32_t prefix_length,
                                         uint32_t num_subnets);
  ~SubnetPool();

  // Allocates and returns a new subnet or nullptr if none are available.
  std::unique_ptr<Subnet> Allocate();

 private:
  SubnetPool(uint32_t base_addr, uint32_t prefix_length, uint32_t num_subnets);

  // Called by Subnets on destruction to free a given subnet.
  void Release(uint32_t index);

  const uint32_t base_addr_;
  const uint32_t prefix_length_;
  const uint32_t num_subnets_;
  const uint32_t addr_per_index_;
  std::bitset<32> subnets_;

  base::WeakPtrFactory<SubnetPool> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SubnetPool);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_SUBNET_POOL_H_
