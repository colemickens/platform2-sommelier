// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/neighbor_cache.h"

#include <linux/neighbour.h>

#include <utility>

#include <base/logging.h>
#include <base/stl_util.h>

namespace portier {

using std::string;

using base::TimeDelta;
using base::TimeTicks;
using shill::IPAddress;

using KeyPair = std::pair<const IPAddress, const string>;
using NeighPair = std::pair<KeyPair, NeighborCacheEntry>;

namespace {

// Amount of time  between an entry being inserted and it being removed
// as obsolete.
constexpr TimeDelta kEntryExpiryTimeout = TimeDelta::FromSeconds(30);

// Checks if the specified |nud_state| is one of the valid NUD states
// recognized by the Linux kernel.  These states are specified in
// <linux/neighbour.h>.  Note that dummy states are not recognized
// as valid.
bool IsValidNudState(uint8_t nud_state) {
  switch (nud_state) {
    case NUD_REACHABLE:
    case NUD_PROBE:
    case NUD_DELAY:
    case NUD_STALE:
    case NUD_INCOMPLETE:
    case NUD_FAILED:
      return true;
  }
  return false;
}

// Converts a NUD state into a relative score used for ranking the
// entries.  The higher the score, the high priority that NUD state
// has when multiple entries can be used.  This score is based on the
// order of preferred states in RFC 4389 Section 4.1.
int GetNudScore(uint8_t nud_state) {
  switch (nud_state) {
    case NUD_REACHABLE:
      return 5;
    case NUD_PROBE:
      return 4;
    case NUD_DELAY:
      return 3;
    case NUD_STALE:
      return 2;
    case NUD_INCOMPLETE:
      return 1;
    case NUD_FAILED:
      return 0;
  }
  return -1;
}

}  // namespace

NeighborCacheEntry::NeighborCacheEntry()
    : is_router(false), nud_state(NUD_NONE) {}

bool NeighborCache::GetEntry(const IPAddress& ip_address,
                             const string& pg_name,
                             NeighborCacheEntry* entry_out) const {
  DCHECK(entry_out);
  auto it = entries_.find(KeyPair(ip_address, pg_name));
  if (it != entries_.end()) {
    *entry_out = it->second;
    return true;
  }
  return false;
}

bool NeighborCache::GetInterfaceRouter(const std::string& if_name,
                                       const std::string& pg_name,
                                       NeighborCacheEntry* entry_out) const {
  DCHECK(entry_out);
  // Using an initial score of 0 to prevent routers in a FAILED states from
  // being returned.
  int nud_score = 0;
  for (const auto& pair : entries_) {
    // Check if potential match, skip if not.
    if (!pair.second.is_router || pair.second.if_name != if_name ||
        pair.first.second != pg_name) {
      continue;
    }
    int new_nud_score = GetNudScore(pair.second.nud_state);
    if (new_nud_score > nud_score) {
      nud_score = new_nud_score;
      *entry_out = pair.second;
    }
  }
  return nud_score > 0;
}

bool NeighborCache::HasEntry(const IPAddress& ip_address,
                             const string& pg_name) const {
  const KeyPair key(ip_address, pg_name);
  return base::ContainsKey(entries_, key);
}

bool NeighborCache::InsertEntry(const string& pg_name,
                                const NeighborCacheEntry& entry,
                                TimeTicks now) {
  // Validating based on needs of IPv6 ND Proxying.
  if (!entry.ip_address.IsValid() ||
      entry.ip_address.family() != IPAddress::kFamilyIPv6 ||
      !entry.ll_address.IsValid() || entry.if_name.size() == 0 ||
      pg_name.size() == 0 || !IsValidNudState(entry.nud_state)) {
    return false;
  }
  const KeyPair key(entry.ip_address, pg_name);
  NeighborCacheEntry& new_entry = entries_[key];
  new_entry = entry;
  entries_[key].expiry_time = now + kEntryExpiryTimeout;
  return true;
}

void NeighborCache::RemoveEntry(const IPAddress& ip_address,
                                const string& pg_name) {
  entries_.erase(KeyPair(ip_address, pg_name));
}

void NeighborCache::ClearForInterface(const string& if_name) {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->second.if_name == if_name) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

void NeighborCache::ClearForGroup(const string& pg_name) {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->first.second == pg_name) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

void NeighborCache::Clear() {
  entries_.clear();
}

void NeighborCache::ClearExpired(TimeTicks now) {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->second.expiry_time <= now) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace portier
