// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/gpo_version_cache.h"

#include <utility>

#include "base/time/clock.h"
#include "base/time/default_clock.h"

#include "bindings/authpolicy_containers.pb.h"

namespace authpolicy {
namespace {
constexpr char kLogHeader[] = "GPO Cache: ";
}

GpoVersionCache::GpoVersionCache(const protos::DebugFlags* flags)
    : flags_(flags), clock_(std::make_unique<base::DefaultClock>()) {}

GpoVersionCache::~GpoVersionCache() = default;

void GpoVersionCache::Add(const std::string& key, uint32_t version) {
  base::Time now = clock_->Now();
  cache_[key] = {version, now};
  LOG_IF(INFO, flags_->log_gpo())
      << kLogHeader << key << ": Adding version " << version << " at " << now;
}

void GpoVersionCache::Remove(const std::string& key) {
  bool erased = cache_.erase(key) != 0;
  if (erased)
    LOG_IF(INFO, flags_->log_gpo()) << kLogHeader << key << ": Removing";
}

bool GpoVersionCache::MayUseCachedGpo(const std::string& key,
                                      uint32_t version) {
  auto it = cache_.find(key);
  if (it == cache_.end()) {
    LOG_IF(INFO, flags_->log_gpo())
        << kLogHeader << key << ": Downloading (not in cache)";
    cache_misses_for_testing_++;
    return false;
  }

  const CacheEntry& cache_entry = it->second;
  if (version != cache_entry.version) {
    LOG_IF(INFO, flags_->log_gpo())
        << kLogHeader << key << ": Downloading (version " << version
        << " != cached version " << cache_entry.version << ")";
    cache_misses_for_testing_++;
    return false;
  }

  LOG_IF(INFO, flags_->log_gpo())
      << kLogHeader << key << ": Using cached version " << cache_entry.version;
  cache_hits_for_testing_++;
  return true;
}

void GpoVersionCache::RemoveEntriesOlderThan(base::TimeDelta max_age) {
  base::Time now = clock_->Now();
  for (auto it = cache_.begin(); it != cache_.end(); /* empty */) {
    // Note: If the clock goes backwards for some reason, clear cache as well
    // just in case the clock was reset.
    const std::string& key = it->first;
    const CacheEntry& cache_entry = it->second;
    base::TimeDelta age = now - cache_entry.cache_time;
    if (age < base::TimeDelta() || age >= max_age) {
      LOG_IF(INFO, flags_->log_gpo())
          << kLogHeader << key << ": Removing from cache (age=" << age << ")";
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}

void GpoVersionCache::SetClockForTesting(std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

}  // namespace authpolicy
