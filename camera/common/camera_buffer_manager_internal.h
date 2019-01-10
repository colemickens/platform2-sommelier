/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_COMMON_CAMERA_BUFFER_MANAGER_INTERNAL_H_
#define CAMERA_COMMON_CAMERA_BUFFER_MANAGER_INTERNAL_H_

#include <gbm.h>

namespace cros {

namespace internal {

struct gbm_device* CreateGbmDevice();

}  // namespace internal

}  // namespace cros

#endif  // CAMERA_COMMON_CAMERA_BUFFER_MANAGER_INTERNAL_H_
