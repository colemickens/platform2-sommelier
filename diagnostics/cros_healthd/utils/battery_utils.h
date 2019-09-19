// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_
#define DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_

#include <vector>

#include <base/macros.h>

#include "debugd/dbus-proxies.h"
#include "mojo/cros_healthd_probe.mojom.h"

namespace diagnostics {

// The BatteryFetcher class is responsible for gathering the battery specific
// metrics reported by cros_healthd. Currently, some metrics are fetched via
// powerd while "smart" battery metrics (ex.  manufacture_date_smart) are
// collected collected from ectool via debugd.
class BatteryFetcher {
 public:
  using BatteryInfoPtr = ::chromeos::cros_healthd::mojom::BatteryInfoPtr;
  using BatteryInfo = ::chromeos::cros_healthd::mojom::BatteryInfo;

  explicit BatteryFetcher(org::chromium::debugdProxyInterface* proxy);
  ~BatteryFetcher();

  // Retrieves the metrics from the main battery over D-Bus.
  std::vector<BatteryInfoPtr> FetchBatteryInfo();

 private:
  // This is a temporary hack enabling the battery_prober to provide smart
  // battery metrics on Sona devices. The proper way to do this will take some
  // planning. Details will be tracked here: https://crbug.com/978615.
  bool FetchManufactureDateSmart(int64_t* manufacture_date_smart);

  // Make a D-Bus call to get the PowerSupplyProperties proto, which contains
  // the battery metrics.
  bool FetchBatteryMetrics(BatteryInfoPtr* output_info);

  // Unowned pointer that should live outside this BatteryFetcher instance.
  org::chromium::debugdProxyInterface* proxy_;

  // Extract the battery metrics from the PowerSupplyProperties protobuf. Return
  // true if the metrics could be successfully extracted from |response| and put
  // it into |output_info|.
  bool ExtractBatteryMetrics(dbus::Response* response,
                             BatteryInfoPtr* output_info);

  friend class BatteryUtilsTest;

  DISALLOW_COPY_AND_ASSIGN(BatteryFetcher);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_
