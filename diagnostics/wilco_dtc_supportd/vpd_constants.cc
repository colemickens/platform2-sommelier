// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/vpd_constants.h"

namespace diagnostics {

// VPD field serial number file path.
const char kVpdFieldSerialNumberFilePath[] =
    "run/wilco_dtc/vpd_fields/serial_number";

// VPD field model name file path.
const char kVpdFieldModelNameFilePath[] = "run/wilco_dtc/vpd_fields/model_name";

// VPD field asset ID file path.
const char kVpdFieldAssetIdFilePath[] = "run/wilco_dtc/vpd_fields/asset_id";

// VPD field SKU number file path.
const char kVpdFieldSkuNumberFilePath[] = "run/wilco_dtc/vpd_fields/sku_number";

// VPD field UUID file path.
const char kVpdFieldUuidFilePath[] = "run/wilco_dtc/vpd_fields/uuid_id";

// VPD field manufacturer date file path.
const char kVpdFieldMfgDateFilePath[] = "run/wilco_dtc/vpd_fields/mfg_date";

// VPD field activate date file path.
const char kVpdFieldActivateDateFilePath[] =
    "run/wilco_dtc/vpd_fields/ActivateDate";

// VPD field system ID file path.
const char kVpdFieldSystemIdFilePath[] = "run/wilco_dtc/vpd_fields/system_id";

}  // namespace diagnostics
