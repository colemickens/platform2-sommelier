// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_SUBNET_H_
#define VM_TOOLS_CONCIERGE_SUBNET_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

namespace vm_tools {
namespace concierge {

// Represents an allocated address inside a subnet.  The address is freed when
// this object is destroyed.
class SubnetAddress {
 public:
  SubnetAddress(uint32_t addr, base::Closure release_cb);
  ~SubnetAddress();

  // Returns this address in network-byte order.
  uint32_t Address() const;

 private:
  // The address in host-byte order.
  uint32_t addr_;

  // Callback to run when this object is destroyed.
  base::Closure release_cb_;

  DISALLOW_COPY_AND_ASSIGN(SubnetAddress);
};

// Represents an allocated subnet.
class Subnet {
 public:
  // Creates a new Subnet with the given network id and prefix.  |release_cb|
  // runs in the destructor of this class and can be used to free other
  // resources associated with the subnet.
  Subnet(uint32_t network_id, size_t prefix, base::Closure release_cb);
  ~Subnet();

  // Marks |addr| as allocated.  |addr| must be in host-byte order.  Returns
  // nullptr if |addr| has already been allocated or if |addr| is not contained
  // within this subnet.  Otherwise, the allocated address is automatically
  // freed when the returned SubnetAddress is destroyed.
  std::unique_ptr<SubnetAddress> Allocate(uint32_t addr);

  // Returns the address at the given |offset| in network byte order. Returns
  // INADDR_ANY if the offset exceeds the available IPs in the subnet.
  // Available IPs do not include the network id or the broadcast address.
  uint32_t AddressAtOffset(uint32_t offset) const;

  // Returns the number of available IPs in this subnet.
  size_t AvailableCount() const;

  // Returns the netmask in network-byte order.
  uint32_t Netmask() const;

  // Returns the prefix.
  size_t Prefix() const;

 private:
  // Marks the address at |offset| as free.
  void Free(uint32_t offset);

  // Subnet network id in host byte order.
  uint32_t network_id_;

  // Prefix.
  size_t prefix_;

  // Keeps track of allocated addresses.
  std::vector<bool> addrs_;

  // Callback to run when this object is deleted.
  base::Closure release_cb_;

  base::WeakPtrFactory<Subnet> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Subnet);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_SUBNET_H_
