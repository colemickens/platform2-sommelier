/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_COMMON_CAMERA_METRICS_IMPL_H_
#define CAMERA_COMMON_CAMERA_METRICS_IMPL_H_

#include <memory>
#include <cros-camera/camera_metrics.h>
#include <metrics/metrics_library.h>

namespace cros {

class CameraMetricsImpl : public CameraMetrics {
 public:
  CameraMetricsImpl();
  ~CameraMetricsImpl() override;

  void SetMetricsLibraryForTesting(
      std::unique_ptr<MetricsLibraryInterface> metrics_lib);

 private:
  std::unique_ptr<MetricsLibraryInterface> metrics_lib_;
};

}  // namespace cros

#endif  // CAMERA_COMMON_CAMERA_METRICS_IMPL_H_
