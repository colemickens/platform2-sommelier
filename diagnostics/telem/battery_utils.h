// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_BATTERY_UTILS_H_
#define DIAGNOSTICS_TELEM_BATTERY_UTILS_H_

#include <memory>

#include "diagnostics/telem/telem_cache.h"

namespace dbus {

class Response;

}  // namespace dbus

namespace diagnostics {

// Extracts the battery metrics from |response| and, if a well-formed
// PowerSupplyProperties protobuf with specifiec fields is obtained, update the
// metrics specified in the parameters: |battery_cycle_count|,
// |battery_vendor|, and |battery_voltage|.
void ExtractBatteryMetrics(dbus::Response* response,
                           base::Optional<base::Value>* battery_cycle_count,
                           base::Optional<base::Value>* battery_vendor,
                           base::Optional<base::Value>* battery_voltage);

// Retrieves the battery metrics from powerd over D-bus. Metrics will be added
// to the |cache|. If |cache| is null, no entries will be added to the cache.
void FetchBatteryMetrics(CacheWriter* cache);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_BATTERY_UTILS_H_
