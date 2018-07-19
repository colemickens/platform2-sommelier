// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/metadata_cache.h"

#include <base/time/tick_clock.h>

namespace smbprovider {

MetadataCache::MetadataCache(base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

MetadataCache::~MetadataCache() = default;

void MetadataCache::AddEntry(const DirectoryEntry& entry) {
  cache_[entry.full_path] = CacheEntry(entry);
}

bool MetadataCache::FindEntry(const std::string& full_path,
                              DirectoryEntry* out_entry) {
  auto entry_iter = cache_.find(full_path);
  if (entry_iter == cache_.end()) {
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

}  // namespace smbprovider
