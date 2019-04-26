// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_TELEM_CACHE_H_
#define DIAGNOSTICS_TELEM_TELEM_CACHE_H_

#include <map>
#include <memory>

#include <base/macros.h>
#include <base/optional.h>
#include <base/time/default_tick_clock.h>
#include <base/time/time.h>
#include <base/time/tick_clock.h>
#include <base/values.h>

#include "diagnostics/telem/telemetry_item_enum.h"

namespace diagnostics {

// CacheWriter provides an interface to enable decoupling of side-effects
// related to caching, from extraction of data from system files.
class CacheWriter {
 public:
  virtual ~CacheWriter();

  // Sets telemetry data for |item|.
  virtual void SetParsedData(TelemetryItemEnum item,
                             base::Optional<base::Value> data) = 0;
};

// Provides caching functionality for libtelem.
class TelemCache final : public CacheWriter {
 public:
  TelemCache();

  // Injects a custom implementation of the TickClock interface.
  // This constructor should only be used for testing. Production
  // code should only call the argument-less constructor.
  explicit TelemCache(base::TickClock* tick_clock);
  ~TelemCache();

  // Whether or not the cache contains valid telemetry data
  // for |item|, which must be no older than |acceptable_age|.
  bool IsValid(TelemetryItemEnum item, base::TimeDelta acceptable_age) const;

  // Gets telemetry data for |item| in an appropriate representation.
  // Does not check that the data is valid, so IsValid(|item|, acceptable_age)
  // should be checked first before calling this function. The returned value
  // should be checked before it is used - the function will return
  // base::nullopt if the requested item does not exist in the cache.
  const base::Optional<base::Value> GetParsedData(TelemetryItemEnum item);

  // Invalidates every item in the cache.
  void Invalidate();

  // CacheWriter overloads:
  // Cached data can be retrieved later via // GetParsedData(|item|).
  void SetParsedData(TelemetryItemEnum item,
                     base::Optional<base::Value> data) override;

 private:
  // Internal representation of the data corresponding to
  // a single telemetry item.
  struct TelemItem {
    TelemItem(base::Optional<base::Value> data_in,
              base::TimeTicks last_fetched_time_ticks_in);
    base::Optional<base::Value> data;
    base::TimeTicks last_fetched_time_ticks;
  };

  std::map<TelemetryItemEnum, TelemItem> cache_;
  std::unique_ptr<base::DefaultTickClock> default_tick_clock_;
  base::TickClock* tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(TelemCache);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_TELEM_CACHE_H_
