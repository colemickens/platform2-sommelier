// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/telemetry/bluetooth_event_service.h"

#include <base/logging.h>

namespace diagnostics {

bool BluetoothEventService::AdapterData::operator==(
    const AdapterData& data) const {
  return name == data.name && address == data.address &&
         powered == data.powered &&
         connected_devices_count == data.connected_devices_count;
}

void BluetoothEventService::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothEventService::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

}  // namespace diagnostics
