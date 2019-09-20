// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_NEIGHBOR_CACHE_H_
#define PORTIER_NEIGHBOR_CACHE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/time/time.h>
#include <shill/net/ip_address.h>

#include "portier/ll_address.h"

namespace portier {

// A subset of the Linux struct neighbour containing only the IPv6 relevant
// information about neighbors.
struct NeighborCacheEntry {
  shill::IPAddress ip_address;
  LLAddress ll_address;
  std::string if_name;
  bool is_router;
  uint8_t nud_state;

  // The time which the entry is considered expired an should be removed.  This
  // time should be updated when and entry is being re-inserted.
  base::TimeTicks expiry_time;

  NeighborCacheEntry();
  ~NeighborCacheEntry() = default;
};

// Manages the cache of neighbour enteries.  Each entry is keyed by
// its IPv6 address and a group name.  The neighbor cache does not
// validate the normal rules of proxy group memberships.
// This class is not thread safe and is intended to run a single threaded
// event loop.
class NeighborCache {
 public:
  NeighborCache() = default;
  ~NeighborCache() = default;

  // Get the neighbor cache entry that is associated to the given
  // |ip_address| and |pg_name|.  Returns false if there is no entry
  // associated to that IP address-group name pair.
  bool GetEntry(const shill::IPAddress& ip_address,
                const std::string& pg_name,
                NeighborCacheEntry* entry_out) const;

  // Get the best router neighbor entry for the provided |if_name|,
  // |pg_name| pair.  The determination of what is "best" is based on
  // the ranking of the NUD state of the cache entry.  In increasing
  // order of priority (defined in RFC4389): INCOMPLETE, STALE, DELAY,
  // PROBE, REACHABLE.  Routers in a FAILED state are not returned.
  bool GetInterfaceRouter(const std::string& if_name,
                          const std::string& pg_name,
                          NeighborCacheEntry* entry_out) const;

  // Returns true if there exists and entry associated to the given
  // |ip_address| and |pg_name| pair.
  bool HasEntry(const shill::IPAddress& ip_address,
                const std::string& pg_name) const;

  // Inserts a new neighbor cache entry, replacing any entry already
  // associated to specified IP address and group name pair.
  // Call will fail if:
  // - |pg_name| is empty.
  // - |entry.ip_address| is not IPv6.
  // - |entry.ll_address| is invalid.
  // - |entry.if_name| is empty.
  // - |entry.nud_state| is not one of the Linux recognized states.
  // The new entry will be assigned an expiry time based on the time
  // provided by the |now| parameter.  This parameter is not validated
  // against existing entries to check if the time is non-decreasing.
  bool InsertEntry(const std::string& pg_name,
                   const NeighborCacheEntry& entry,
                   base::TimeTicks now = base::TimeTicks::Now());

  // Clear the specific entry assocated to the provided |ip_address|.
  void RemoveEntry(const shill::IPAddress& ip_address,
                   const std::string& pg_name);

  // Removes all of the neighbor cache entries associated to a
  // specified interface name.  Useful if an interface is destroyed
  // or removed from a group.
  void ClearForInterface(const std::string& if_name);

  // Removes all of the neighbor cache entries associated to a
  // specified group name.  Useful if a group is destroyed.
  void ClearForGroup(const std::string& pg_name);

  // Clears out the entire cache.
  void Clear();

  // Clears all entries which have an |entry.expiry_time| less than
  // or equal to the provided |now| time.
  void ClearExpired(base::TimeTicks now = base::TimeTicks::Now());

 private:
  // Maps the pair of the string representation of the IPv6 address
  // and the group name to a neighbor cache entry.
  std::map<std::pair<shill::IPAddress, std::string>, NeighborCacheEntry>
      entries_;
};

}  // namespace portier

#endif  // PORTIER_NEIGHBOR_CACHE_H_
