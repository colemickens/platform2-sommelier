// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_
#define DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_

#include <vector>

#include "mojo/cros_healthd_probe.mojom.h"

namespace diagnostics {

// Retrieves the main battery metrics from powerd over D-bus.
std::vector<chromeos::cros_healthd::mojom::BatteryInfoPtr> FetchBatteryInfo();

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_
