// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_SUBNET_POOL_H_
#define VM_TOOLS_CONCIERGE_SUBNET_POOL_H_

#include <stdint.h>

#include <bitset>
#include <memory>
#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

namespace vm_tools {
namespace concierge {

// Manages IPv4 subnets that can be assigned to virtual machines.  VMs are
// assigned non-publicly routable addresses in the range 100.115.92.0/24.
class SubnetPool {
 public:
  SubnetPool();
  ~SubnetPool();

  // Represents an allocated subnet in the range 100.115.92.0/24.
  class Subnet {
   public:
    ~Subnet();

    // Returns the gateway address for this subnet.
    std::string GatewayAddress() const;

    // Returns the assigned IPv4 address.
    std::string IPv4Address() const;

    // Returns the netmask.
    std::string Netmask() const;

   private:
    friend class SubnetPool;
    Subnet(size_t index, base::Closure release_cb);

    // The index of the subnet assigned to this object.
    size_t index_;

    // Callback to run when this object is deleted.
    base::Closure release_cb_;

    DISALLOW_COPY_AND_ASSIGN(Subnet);
  };

  // Create a new subnet for testing.
  std::unique_ptr<Subnet> CreateForTesting(size_t index);

  // Allocates and returns a new Subnet in the range 100.115.92.0/24.  Returns
  // nullptr if no subnets are available.
  std::unique_ptr<Subnet> Allocate();

 private:
  // Called by Subnets on destruction to free a given subnet.
  void Release(size_t index);

  // The /24 subnet is split up into 64 /30 subnets before being handed out.
  std::bitset<64> subnets_;

  base::WeakPtrFactory<SubnetPool> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SubnetPool);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_SUBNET_POOL_H_
