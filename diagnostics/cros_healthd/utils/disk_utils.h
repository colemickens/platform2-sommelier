// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_UTILS_DISK_UTILS_H_
#define DIAGNOSTICS_CROS_HEALTHD_UTILS_DISK_UTILS_H_

#include <vector>

#include "mojo/cros_healthd_probe.mojom.h"

namespace diagnostics {
namespace disk_utils {

std::vector<chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr>
FetchNonRemovableBlockDevicesInfo(const base::FilePath& root_dir);

}  // namespace disk_utils
}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_UTILS_DISK_UTILS_H_
