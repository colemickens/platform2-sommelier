/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/camera_metrics_impl.h"

#include <utility>

namespace cros {

// static
std::unique_ptr<CameraMetrics> CameraMetrics::New() {
  return std::make_unique<CameraMetricsImpl>();
}

CameraMetricsImpl::CameraMetricsImpl()
    : metrics_lib_(std::make_unique<MetricsLibrary>()) {}

CameraMetricsImpl::~CameraMetricsImpl() = default;

void CameraMetricsImpl::SetMetricsLibraryForTesting(
    std::unique_ptr<MetricsLibraryInterface> metrics_lib) {
  metrics_lib_ = std::move(metrics_lib);
}

}  // namespace cros
