// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/telemetry/fake_bluetooth_event_service.h"

namespace diagnostics {

FakeBluetoothEventService::FakeBluetoothEventService() = default;

FakeBluetoothEventService::~FakeBluetoothEventService() = default;

void FakeBluetoothEventService::EmitBluetoothAdapterDataChanged(
    const std::vector<BluetoothEventService::AdapterData>& adapters) const {
  for (auto& observer : observers_) {
    observer.BluetoothAdapterDataChanged(adapters);
  }
}

}  // namespace diagnostics
