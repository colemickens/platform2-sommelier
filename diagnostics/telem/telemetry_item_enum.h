// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_TELEMETRY_ITEM_ENUM_H_
#define DIAGNOSTICS_TELEM_TELEMETRY_ITEM_ENUM_H_

namespace diagnostics {

// TODO(pmoy@chromium.org): As telemetry items are finalized,
// make sure the units being measured for each item is clear.

// Enumerates each of the individual telemetry items that
// can be requested from libtelem.
enum class TelemetryItemEnum {
  kUptime,
  kMemTotalMebibytes,
  kMemFreeMebibytes,
  kLoadAvg,
  kStat,
  kAcpiButton,
  kNetStat,
  kNetDev
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_TELEMETRY_ITEM_ENUM_H_
