/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_CROS_CAMERA_TIMEZONE_H_
#define INCLUDE_CROS_CAMERA_TIMEZONE_H_

#include <string>

#include "cros-camera/export.h"

namespace cros {

enum class PowerLineFrequency {
  FREQ_DEFAULT = 0,
  FREQ_50HZ = 1,
  FREQ_60HZ = 2,
  FREQ_AUTO = 3,
  FREQ_ERROR = 4,
};

// Checks the system timezone and turns it into a two-character ASCII country
// code. This may fail (for example, it will always fail on Android), in which
// case it will return an empty string.
CROS_CAMERA_EXPORT std::string CountryCodeForCurrentTimezone();

// Queries timezone to know the country to decide power frequency to do
// anti-banding.
CROS_CAMERA_EXPORT PowerLineFrequency GetPowerLineFrequencyForLocation();

}  // namespace cros

#endif  // INCLUDE_CROS_CAMERA_TIMEZONE_H_
