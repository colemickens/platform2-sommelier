// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_TELEMETRY_SERVICE_IMPL_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_TELEMETRY_SERVICE_IMPL_H_

#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/optional.h>
#include <base/values.h>

#include "diagnostics/cros_healthd/cros_healthd_telemetry_service.h"
#include "diagnostics/telem/telemetry_group_enum.h"
#include "diagnostics/telem/telemetry_item_enum.h"

namespace diagnostics {

// Production implementation of the CrosHealthdTelemetryService interface.
class CrosHealthdTelemetryServiceImpl final
    : public CrosHealthdTelemetryService {
 public:
  CrosHealthdTelemetryServiceImpl();
  ~CrosHealthdTelemetryServiceImpl() override;

  // CrosHealthdTelemetryService overrides:
  base::Optional<base::Value> GetTelemetryItem(TelemetryItemEnum item) override;
  std::vector<std::pair<TelemetryItemEnum, base::Optional<base::Value>>>
  GetTelemetryGroup(TelemetryGroupEnum group) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosHealthdTelemetryServiceImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_TELEMETRY_SERVICE_IMPL_H_
