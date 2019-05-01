// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_TELEMETRY_SERVICE_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_TELEMETRY_SERVICE_H_

#include <utility>
#include <vector>

#include <base/optional.h>
#include <base/values.h>

#include "diagnostics/telem/telemetry_group_enum.h"
#include "diagnostics/telem/telemetry_item_enum.h"

namespace diagnostics {

// Service responsible for fetching telemetry data from the device.
class CrosHealthdTelemetryService {
 public:
  virtual ~CrosHealthdTelemetryService() = default;

  // Returns telemetry data corresponding to |item|. The returned value should
  // be checked before it is used - the function will return base::nullopt if
  // the requested item could not be retrieved.
  virtual base::Optional<base::Value> GetTelemetryItem(
      TelemetryItemEnum item) = 0;
  // Returns telemetry data for each item in |group|.
  virtual std::vector<std::pair<TelemetryItemEnum, base::Optional<base::Value>>>
  GetTelemetryGroup(TelemetryGroupEnum group) = 0;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_TELEMETRY_SERVICE_H_
