// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/metadata_cache.h"

#include <base/time/tick_clock.h>

namespace smbprovider {

MetadataCache::MetadataCache(base::TickClock* tick_clock,
                             base::TimeDelta entry_lifetime)
    : tick_clock_(tick_clock), entry_lifetime_(entry_lifetime) {}

MetadataCache::~MetadataCache() = default;

void MetadataCache::AddEntry(const DirectoryEntry& entry) {
  cache_[entry.full_path] =
      CacheEntry(entry, tick_clock_->NowTicks() + entry_lifetime_);
}

bool MetadataCache::FindEntry(const std::string& full_path,
                              DirectoryEntry* out_entry) {
  auto entry_iter = cache_.find(full_path);
  if (entry_iter == cache_.end()) {
    return false;
  }

  if (IsExpired(entry_iter->second)) {
    cache_.erase(entry_iter);
    return false;
  }

  *out_entry = entry_iter->second.entry;
  return true;
}

void MetadataCache::ClearAll() {
  cache_.clear();
}

bool MetadataCache::IsEmpty() const {
  return cache_.empty();
}

bool MetadataCache::RemoveEntry(const std::string& entry_path) {
  return cache_.erase(entry_path) > 0;
}

bool MetadataCache::IsExpired(const MetadataCache::CacheEntry& cache_entry) {
  return tick_clock_->NowTicks() > cache_entry.expiration_time;
}

}  // namespace smbprovider
