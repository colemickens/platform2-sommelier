/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMMON_UTILS_CAMERA_HAL_ENUMERATOR_H_
#define COMMON_UTILS_CAMERA_HAL_ENUMERATOR_H_

#include <vector>

#include <base/files/file_path.h>

namespace cros {

std::vector<base::FilePath> GetCameraHalPaths();

}  // namespace cros

#endif  // COMMON_UTILS_CAMERA_HAL_ENUMERATOR_H_
