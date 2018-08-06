/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_REPROCESS_EFFECT_SCOPED_BUFFER_HANDLE_H_
#define HAL_ADAPTER_REPROCESS_EFFECT_SCOPED_BUFFER_HANDLE_H_

#include <base/macros.h>

#include "cros-camera/camera_buffer_manager.h"

namespace cros {

class ScopedYUVBufferHandle {
 public:
  static ScopedYUVBufferHandle CreateScopedHandle(buffer_handle_t handle,
                                                  uint32_t flag,
                                                  uint32_t width,
                                                  uint32_t height);

  ScopedYUVBufferHandle(ScopedYUVBufferHandle&& other);

  ~ScopedYUVBufferHandle();

  operator bool() const;

  const struct android_ycbcr* GetYCbCr();

 private:
  ScopedYUVBufferHandle();

  buffer_handle_t handle_;

  cros::CameraBufferManager* buffer_manager_;

  struct android_ycbcr ycbcr_;

  DISALLOW_COPY_AND_ASSIGN(ScopedYUVBufferHandle);
};

}  // namespace cros

#endif  // HAL_ADAPTER_REPROCESS_EFFECT_SCOPED_BUFFER_HANDLE_H_
