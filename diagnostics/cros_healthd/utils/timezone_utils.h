// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_UTILS_TIMEZONE_UTILS_H_
#define DIAGNOSTICS_CROS_HEALTHD_UTILS_TIMEZONE_UTILS_H_

#include <base/files/file_path.h>

#include "mojo/cros_healthd_probe.mojom.h"

namespace diagnostics {

chromeos::cros_healthd::mojom::TimezoneInfoPtr FetchTimezoneInfo(
    const base::FilePath& root_dir);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_UTILS_TIMEZONE_UTILS_H_
