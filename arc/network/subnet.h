// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_SUBNET_H_
#define ARC_NETWORK_SUBNET_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <brillo/brillo_export.h>

namespace arc_networkd {

// Represents an allocated address inside a subnet.  The address is freed when
// this object is destroyed.
class BRILLO_EXPORT SubnetAddress {
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
class BRILLO_EXPORT Subnet {
 public:
  // Creates a new Subnet with the given network id and prefix. |release_cb|
  // runs in the destructor of this class and can be used to free other
  // resources associated with the subnet.
  Subnet(uint32_t network_id, uint32_t prefix, base::Closure release_cb);
  ~Subnet();

  // Marks |addr| as allocated. |addr| must be in host-byte order. Returns
  // nullptr if |addr| has already been allocated or if |addr| is not contained
  // within this subnet. Otherwise, the allocated address is automatically
  // freed when the returned SubnetAddress is destroyed.
  std::unique_ptr<SubnetAddress> Allocate(uint32_t addr);

  // Allocates the address at |offset|. Returns nullptr if |offset| is invalid
  // (exceeds available IPs in the subnet) or is already allocated.
  std::unique_ptr<SubnetAddress> AllocateAtOffset(uint32_t offset);

  // Returns the address at the given |offset| in network byte order. Returns
  // INADDR_ANY if the offset exceeds the available IPs in the subnet.
  // Available IPs do not include the network id or the broadcast address.
  uint32_t AddressAtOffset(uint32_t offset) const;

  // Returns the number of available IPs in this subnet.
  uint32_t AvailableCount() const;

  // Returns the netmask in network-byte order.
  uint32_t Netmask() const;

  // Returns the prefix.
  uint32_t Prefix() const;

 private:
  // Marks the address at |offset| as free.
  void Free(uint32_t offset);

  // Subnet network id in host byte order.
  uint32_t network_id_;

  // Prefix.
  uint32_t prefix_;

  // Keeps track of allocated addresses.
  std::vector<bool> addrs_;

  // Callback to run when this object is deleted.
  base::Closure release_cb_;

  base::WeakPtrFactory<Subnet> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Subnet);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_SUBNET_H_
