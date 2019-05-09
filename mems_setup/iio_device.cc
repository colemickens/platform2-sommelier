// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mems_setup/iio_device.h"

namespace mems_setup {

bool IioDevice::IsSingleSensor() const {
  return ReadStringAttribute("location").has_value();
}

}  // namespace mems_setup
