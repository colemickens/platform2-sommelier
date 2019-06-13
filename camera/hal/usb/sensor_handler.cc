/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/sensor_handler.h"

#include "cros-camera/common.h"
#if defined(MONOCLE_QUIRKS)
#include "hal/usb/sensor_handler_monocle.h"
#endif

namespace cros {

// static
std::unique_ptr<SensorHandler> SensorHandler::Create(
    const DeviceInfo& device_info, const SupportedFormats& supported_formats) {
#if defined(MONOCLE_QUIRKS)
  return std::make_unique<SensorHandlerMonocle>(device_info, supported_formats);
#else
  return std::make_unique<SensorHandlerDefault>();
#endif
}

int64_t SensorHandlerDefault::GetRollingShutterSkew(const Size& resolution) {
  return 33'300'000;
}

int64_t SensorHandlerDefault::GetExposureTime(const Size& resolution) {
  return 16'600'000;
}

}  // namespace cros
