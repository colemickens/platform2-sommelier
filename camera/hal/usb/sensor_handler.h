/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_USB_SENSOR_HANDLER_H_
#define CAMERA_HAL_USB_SENSOR_HANDLER_H_

#include <memory>

#include "hal/usb/common_types.h"

namespace cros {

class SensorHandler {
 public:
  static std::unique_ptr<SensorHandler> Create(
      const DeviceInfo& device_info, const SupportedFormats& supported_formats);
  virtual ~SensorHandler() {}

  // Get rolling shutter skew value. The return value unit is nano seconds.
  virtual int64_t GetRollingShutterSkew(const Size& resolution) = 0;

  // Get exposure time. The return value unit is nano seconds.
  virtual int64_t GetExposureTime(const Size& resolution) = 0;
};

class SensorHandlerDefault : public SensorHandler {
 public:
  SensorHandlerDefault() {}
  ~SensorHandlerDefault() override = default;

  int64_t GetRollingShutterSkew(const Size& resolution) override;
  int64_t GetExposureTime(const Size& resolution) override;
};

}  // namespace cros

#endif  // CAMERA_HAL_USB_SENSOR_HANDLER_H_
