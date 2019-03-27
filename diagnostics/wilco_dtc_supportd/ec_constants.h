// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_EC_CONSTANTS_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_EC_CONSTANTS_H_

#include <cstdint>

namespace diagnostics {

extern const char kEcDriverSysfsPath[];

extern const char kEcDriverSysfsPropertiesPath[];

extern const int64_t kEcGetTelemetryPayloadMaxSize;

extern const char kEcGetTelemetryFilePath[];

extern const char kEcEventFilePath[];

extern const int16_t kEcEventFilePollEvents;

extern const char kEcPropertyGlobalMicMuteLed[];

extern const char kEcPropertyFnLock[];

extern const char kEcPropertyNic[];

extern const char kEcPropertyExtUsbPortEn[];

extern const char kEcPropertyWirelessSwWlan[];

extern const char kEcPropertyAutoBootOnTrinityDockAttach[];

extern const char kEcPropertyIchAzaliaEn[];

extern const char kEcPropertySignOfLifeKbbl[];

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_EC_CONSTANTS_H_
