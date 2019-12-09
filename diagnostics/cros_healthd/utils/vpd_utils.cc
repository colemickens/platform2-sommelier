// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/vpd_utils.h"

#include "diagnostics/cros_healthd/utils/file_utils.h"

namespace diagnostics {

namespace {

using ::chromeos::cros_healthd::mojom::CachedVpdInfo;
using ::chromeos::cros_healthd::mojom::CachedVpdInfoPtr;

}  // namespace

CachedVpdInfoPtr FetchCachedVpdInfo(const base::FilePath& root_dir) {
  constexpr char kRelativeSkuNumberDir[] = "sys/firmware/vpd/ro/";
  constexpr char kSkuNumberFileName[] = "sku_number";
  CachedVpdInfo vpd_info;
  ReadAndTrimString(root_dir.Append(kRelativeSkuNumberDir), kSkuNumberFileName,
                    &vpd_info.sku_number);
  return vpd_info.Clone();
}

}  // namespace diagnostics
