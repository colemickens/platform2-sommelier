// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_BLUETOOTH_EVENT_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_BLUETOOTH_EVENT_SERVICE_H_

#include <vector>

#include <base/macros.h>

#include "diagnostics/wilco_dtc_supportd/telemetry/bluetooth_event_service.h"

namespace diagnostics {

class FakeBluetoothEventService : public BluetoothEventService {
 public:
  FakeBluetoothEventService();
  ~FakeBluetoothEventService() override;

  void EmitBluetoothAdapterDataChanged(
      const std::vector<BluetoothEventService::AdapterData>& adapters) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothEventService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_BLUETOOTH_EVENT_SERVICE_H_
