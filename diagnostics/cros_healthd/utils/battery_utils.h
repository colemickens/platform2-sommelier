// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_
#define DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <dbus/object_proxy.h>

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

  BatteryFetcher(org::chromium::debugdProxyInterface* debugd_proxy,
                 dbus::ObjectProxy* power_manager_proxy);
  ~BatteryFetcher();

  // Retrieves the metrics from the main battery over D-Bus.
  std::vector<BatteryInfoPtr> FetchBatteryInfo();

 private:
  // Currently, the battery_prober provides the manufacture_date_smart and
  // temperature_smart property on Sona and Careena devices. Eventualy, this
  // property will be reported for all devices. Details will be tracked here:
  // https://crbug.com/978615. The |metric_name| identifies the smart battery
  // metric cros_healthd wants to request from debugd. Once debugd retrieves
  // this value via ectool, it populates |smart_metric|.
  template <typename T>
  bool FetchSmartBatteryMetric(
      const std::string& metric_name,
      T* smart_metric,
      base::OnceCallback<bool(const base::StringPiece& input, T* output)>
          convert_string_to_num);

  // Make a D-Bus call to get the PowerSupplyProperties proto, which contains
  // the battery metrics.
  bool FetchBatteryMetrics(BatteryInfoPtr* output_info);

  // Unowned pointer that outlives this BatteryFetcher instance.
  org::chromium::debugdProxyInterface* debugd_proxy_;

  // Unowned pointer that outlives this BatteryFetcher instance.
  dbus::ObjectProxy* power_manager_proxy_;

  // Extract the battery metrics from the PowerSupplyProperties protobuf. Return
  // true if the metrics could be successfully extracted from |response| and put
  // it into |output_info|.
  bool ExtractBatteryMetrics(dbus::Response* response,
                             BatteryInfoPtr* output_info);

  DISALLOW_COPY_AND_ASSIGN(BatteryFetcher);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_UTILS_BATTERY_UTILS_H_
