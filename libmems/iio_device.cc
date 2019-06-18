// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libmems/iio_device.h"

namespace libmems {

bool IioDevice::IsSingleSensor() const {
  return ReadStringAttribute("location").has_value();
}

}  // namespace libmems
