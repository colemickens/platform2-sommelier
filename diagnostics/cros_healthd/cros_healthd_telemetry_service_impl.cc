// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/cros_healthd_telemetry_service_impl.h"

#include <base/logging.h>

namespace diagnostics {

CrosHealthdTelemetryServiceImpl::CrosHealthdTelemetryServiceImpl() = default;
CrosHealthdTelemetryServiceImpl::~CrosHealthdTelemetryServiceImpl() = default;

base::Optional<base::Value> CrosHealthdTelemetryServiceImpl::GetTelemetryItem(
    TelemetryItemEnum item) {
  NOTIMPLEMENTED();
  return base::Optional<base::Value>();
}

std::vector<std::pair<TelemetryItemEnum, base::Optional<base::Value>>>
CrosHealthdTelemetryServiceImpl::GetTelemetryGroup(TelemetryGroupEnum group) {
  NOTIMPLEMENTED();
  return std::vector<
      std::pair<TelemetryItemEnum, base::Optional<base::Value>>>();
}

}  // namespace diagnostics
