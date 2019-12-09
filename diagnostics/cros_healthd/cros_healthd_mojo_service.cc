// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/cros_healthd_mojo_service.h"

#include <string>
#include <utility>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/optional.h>
#include <dbus/cros_healthd/dbus-constants.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/interface_request.h>

#include "diagnostics/cros_healthd/utils/battery_utils.h"
#include "diagnostics/cros_healthd/utils/disk_utils.h"
#include "diagnostics/cros_healthd/utils/vpd_utils.h"

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

CrosHealthdMojoService::CrosHealthdMojoService(BatteryFetcher* battery_fetcher)
    : battery_fetcher_(battery_fetcher) {
  DCHECK(battery_fetcher_);
}

CrosHealthdMojoService::~CrosHealthdMojoService() = default;

void CrosHealthdMojoService::ProbeTelemetryInfo(
    const std::vector<ProbeCategoryEnum>& categories,
    const ProbeTelemetryInfoCallback& callback) {
  chromeos::cros_healthd::mojom::TelemetryInfo telemetry_info;
  for (const auto category : categories) {
    switch (category) {
      case ProbeCategoryEnum::kBattery: {
        auto batteries = battery_fetcher_->FetchBatteryInfo();
        telemetry_info.battery_info.Swap(&batteries[0]);
        break;
      }
      case ProbeCategoryEnum::kCachedVpdData: {
        auto vpd_info = FetchCachedVpdInfo(base::FilePath("/"));
        telemetry_info.vpd_info.Swap(&vpd_info);
        break;
      }
      case ProbeCategoryEnum::kNonRemovableBlockDevices: {
        telemetry_info.block_device_info = base::Optional<std::vector<
            chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr>>(
            FetchNonRemovableBlockDevicesInfo(base::FilePath("/")));
        break;
      }
    }
  }

  callback.Run(telemetry_info.Clone());
}

void CrosHealthdMojoService::AddBinding(
    chromeos::cros_healthd::mojom::CrosHealthdServiceRequest request) {
  binding_set_.AddBinding(this /* impl */, std::move(request));
}

}  // namespace diagnostics
