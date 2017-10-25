/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMMON_CAMERA_BUFFER_MANAGER_INTERNAL_H_
#define COMMON_CAMERA_BUFFER_MANAGER_INTERNAL_H_

#include <gbm.h>

namespace arc {

namespace internal {

struct gbm_device* CreateGbmDevice();

}  // namespace internal

}  // namespace arc

#endif  // COMMON_CAMERA_BUFFER_MANAGER_INTERNAL_H_
