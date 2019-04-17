/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_INCLUDE_CROS_CAMERA_CAMERA_METRICS_H_
#define CAMERA_INCLUDE_CROS_CAMERA_CAMERA_METRICS_H_

#include <memory>

#include "cros-camera/export.h"

namespace cros {

class CROS_CAMERA_EXPORT CameraMetrics {
 public:
  static std::unique_ptr<CameraMetrics> New();

  virtual ~CameraMetrics() = default;
};

}  // namespace cros

#endif  // CAMERA_INCLUDE_CROS_CAMERA_CAMERA_METRICS_H_
