// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_UTIL_H_
#define BLUETOOTH_NEWBLUED_UTIL_H_

#include <newblue/gatt.h>

#include <memory>

#include "bluetooth/newblued/gatt_attributes.h"

namespace bluetooth {

// Converts struct GattTraversedService to bluetooth::GattService.
std::unique_ptr<GattService> ConvertToGattService(
    const struct GattTraversedService& service);

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_UTIL_H_
