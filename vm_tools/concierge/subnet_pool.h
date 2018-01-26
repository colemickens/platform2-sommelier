// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_SUBNET_POOL_H_
#define VM_TOOLS_CONCIERGE_SUBNET_POOL_H_

#include <stdint.h>

#include <bitset>
#include <memory>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

namespace vm_tools {
namespace concierge {

// Manages IPv4 subnets that can be assigned to virtual machines and containers.
// These use non-publicly routable addresses in the range 100.115.92.0/24.
class SubnetPool {
 public:
  SubnetPool();
  ~SubnetPool();

  // Represents an allocated subnet in the range 100.115.92.0/24.
  class Subnet {
   public:
    ~Subnet();

    // Returns the available IPv4 addresses in network byte order. Returns
    // INADDR_ANY if the offset exceeds the available IPs in the subnet.
    // Available IPs do not include the network identifier or the broadcast
    // address.
    uint32_t AddressAtOffset(uint32_t offset) const;

    // Returns the number of available IPs in this subnet.
    size_t AvailableCount() const;

    // Returns the netmask.
    uint32_t Netmask() const;

    // Returns the prefix.
    size_t Prefix() const;

   private:
    friend class SubnetPool;
    Subnet(uint32_t network_id, size_t prefix, base::Closure release_cb);

    // Subnet network id in host byte order.
    uint32_t network_id_;

    // Prefix.
    size_t prefix_;

    // Callback to run when this object is deleted.
    base::Closure release_cb_;

    DISALLOW_COPY_AND_ASSIGN(Subnet);
  };

  // Create a new subnet for testing.
  std::unique_ptr<Subnet> CreateVMForTesting(size_t index);

  // Create a new subnet for testing.
  std::unique_ptr<Subnet> CreateContainerForTesting(size_t index);

  // Allocates and returns a new VM Subnet in the range 100.115.92.0/24. Returns
  // nullptr if no subnets are available.
  std::unique_ptr<Subnet> AllocateVM();

  // Allocates and returns a new Container Subnet in the range 100.115.92.0/24.
  // Returns nullptr if no subnets are available.
  std::unique_ptr<Subnet> AllocateContainer();

 private:
  // Called by Subnets on destruction to free a given subnet.
  void ReleaseVM(size_t index);

  // Called by Subnets on destruction to free a given subnet.
  void ReleaseContainer(size_t index);

  // There are 31 /30 subnets.
  std::bitset<32> vm_subnets_;

  // There are 4 /28 subnets.
  std::bitset<4> container_subnets_;

  base::WeakPtrFactory<SubnetPool> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SubnetPool);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_SUBNET_POOL_H_
