// Copyright 2018 The Chromium OS Authors. All rights reserved.
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

#include "diagnostics/cros_healthd/utils/battery_utils.h"
#include "diagnostics/cros_healthd/utils/disk_utils.h"

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

CrosHealthdMojoService::CrosHealthdMojoService(
    mojo::ScopedMessagePipeHandle mojo_pipe_handle)
    : self_binding_(this /* impl */, std::move(mojo_pipe_handle)) {
  DCHECK(self_binding_.is_bound());
}

CrosHealthdMojoService::~CrosHealthdMojoService() = default;

void CrosHealthdMojoService::set_connection_error_handler(
    const base::Closure& error_handler) {
  self_binding_.set_connection_error_handler(error_handler);
}

void CrosHealthdMojoService::ProbeTelemetryInfo(
    const std::vector<ProbeCategoryEnum>& categories,
    const ProbeTelemetryInfoCallback& callback) {
  chromeos::cros_healthd::mojom::TelemetryInfo telemetry_info;
  for (const auto category : categories) {
    switch (category) {
      case ProbeCategoryEnum::kBattery: {
        auto batteries = FetchBatteryInfo();
        telemetry_info.battery_info.Swap(&batteries[0]);
        break;
      }
      case ProbeCategoryEnum::kCachedVpdData: {
        auto vpd_info = disk_utils::FetchCachedVpdInfo(base::FilePath("/"));
        telemetry_info.vpd_info.Swap(&vpd_info);
        break;
      }
      case ProbeCategoryEnum::kNonRemovableBlockDevices: {
        telemetry_info.block_device_info = base::Optional<std::vector<
            chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr>>(
            disk_utils::FetchNonRemovableBlockDevicesInfo(base::FilePath("/")));
        break;
      }
    }
  }

  callback.Run(telemetry_info.Clone());
}

}  // namespace diagnostics
